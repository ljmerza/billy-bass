/*
  Big Mouth Billy Bass - Sound Reactive Animatronic
  Originally by Donald Bell, Maker Project Lab (2016).
  Based on Sound to Servo by Cenk Ozdemir (2012)
  and DCMotorTest by Adafruit
*/

#include <Wire.h>
#include <Adafruit_MotorShield.h>
#include "utility/Adafruit_MS_PWMServoDriver.h"
#include "wifi_ota.h"
#include "swatchdog.h"
#include "failsafe.h"
#include "status_display.h"
#include "config.h"
#include "telemetry.h"
#include "netlog.h"
#include "http_api.h"
#include "mqtt_ha.h"
#include "speech.h"
#include "sound.h"
#include "motortest.h"

// ----- Pin Configuration -----
const int SOUND_PIN = A0;               // Analog pin for sound sensor input

// ----- Motor Configuration -----
const int MOUTH_MOTOR_PORT = 1;         // Motor shield port for mouth
const int HEAD_MOTOR_PORT  = 2;         // Motor shield port for head
const int TAIL_MOTOR_PORT  = 3;         // Motor shield port for tail
const int HEAD_SPEED       = 254;       // Head drive speed during the stroke (0-255)
const unsigned long HEAD_MOVE_MS = 400; // Drive stroke length - sets how far the head swings out
const int HEAD_HOLD_SPEED  = 120;       // Speed that holds the head out afterwards (0 = let it fall back)
const int MOUTH_SPEED_MIN  = 140;       // Minimum mouth motor speed when sound detected
const int MOUTH_SPEED_MAX  = 254;       // Maximum mouth motor speed at loudest sound
const int TAIL_SPEED       = 200;       // Tail motor speed while flapping (0-255)
const unsigned long TAIL_FLAP_MS = 150; // Drive stroke, then an equal release stroke

// Every motor here drives one way against a return spring and a hard stop. The
// spring provides the return; a reversal just stalls the motor into the stop and
// strips the gear train. Nothing in this sketch calls run(BACKWARD).

// ----- Sound Detection -----
const int SOUND_THRESHOLD  = 30;        // Minimum mapped value to trigger motors
const int SENSOR_RAW_MAX   = 512;       // Upper range of raw analog reading
const int SENSOR_MAP_MAX   = 180;       // Upper range after mapping

// ----- Timing -----
const unsigned long HEAD_TIMEOUT_MS = 3000;  // How long the head stays out after the last sound

// ----- Debug -----
const bool DEBUG = true;                     // Toggle serial debug output

// ----- Supervision -----
const unsigned long WATCHDOG_TIMEOUT_MS = 20000;  // loop() silence before reset

// ----- Network services -----
const uint16_t HTTP_PORT = 80;
const uint16_t LOG_PORT  = 9999;              // UDP broadcast; listen with: nc -ul 9999
const unsigned long TELEMETRY_INTERVAL_MS = 1000;

// The motor tunables above are defaults only. Live values come from config(),
// which restores whatever was last set over HTTP or MQTT.
const BassConfig DEFAULT_CONFIG = {
  SOUND_THRESHOLD, MOUTH_SPEED_MIN, MOUTH_SPEED_MAX,
  HEAD_SPEED, HEAD_MOVE_MS, HEAD_HOLD_SPEED, HEAD_TIMEOUT_MS,
  TAIL_SPEED, TAIL_FLAP_MS,
  0,      // speakDelayMs - raise to line the mouth up with audio played elsewhere
  100,    // speakRatePct - 100 is normal speaking pace
  300,    // ppFullScale - raw ADC swing treated as full volume
  1,      // voiceFilter - measure the 300-3000Hz band only
  1,      // articulate - pulse the mouth rather than holding it open
  90,     // mouthOnMs - length of one mouth pulse
  70,     // mouthOffMs - guaranteed closed time between pulses
  115     // adaptPct - open on peaks 15% above the running mean
};

// ----- Globals -----
Adafruit_MotorShield AFMS = Adafruit_MotorShield();
Adafruit_DCMotor *mouthMotor = AFMS.getMotor(MOUTH_MOTOR_PORT);
Adafruit_DCMotor *headMotor  = AFMS.getMotor(HEAD_MOTOR_PORT);
Adafruit_DCMotor *tailMotor  = AFMS.getMotor(TAIL_MOTOR_PORT);
unsigned long lastSoundTime = 0;

// Head state. headDriveAt is when the current stroke started, so the drive and
// hold halves can be told apart without a second flag.
bool headActive = false;
unsigned long headDriveAt = 0;

// Tail flap state. tailFlapping is the whole cycle being live; tailDriving is
// which half of it we are in - motor pulling, or released so the spring returns.
bool tailFlapping = false;
bool tailDriving = false;
unsigned long tailPhaseAt = 0;

// Mouth articulation state. mouthDriving is a pulse in progress; mouthReadyAt
// is the earliest the next one may start, which is what guarantees the return
// spring a closed window to work in.
bool mouthDriving = false;
unsigned long mouthPulseUntil = 0;
unsigned long mouthReadyAt = 0;
int mouthPulseSpeed = 0;

// Stops all three motors. Passed to wifiOtaSetup(), which calls it before anything
// that stalls or resets the sketch - an OTA transfer, the flash copy before the
// reboot, or a blocking WiFi connect attempt. The motor shield keeps its PWM
// registers across an MCU reset, so releasing here is what holds the fish still
// until setup() runs AFMS.begin() again.
void releaseMotors() {
  speechStop();             // abandon any utterance rather than resume mid-word
  mouthMotor->run(RELEASE);
  headMotor->run(RELEASE);
  tailMotor->run(RELEASE);
  headActive = false;
  mouthDriving = false;
  tailFlapping = false;
  tailDriving = false;
}

// Swings the head out and holds it there. Nothing reports where the head
// actually is, so travel is timed instead of measured: cfg.headSpeed for
// cfg.headMoveMs is the stroke, and a longer stroke pulls the head further out
// until it meets its stop. The speed then drops to cfg.headHoldSpeed, enough to
// hold against the return spring without stalling the gear train at full power
// for the whole time the head is out. Once cfg.headTimeoutMs has passed since
// the last sound the motor releases and the spring takes the head back in.
//
// Setting the speed every pass is also what makes cfg.headSpeed take effect
// live - it used to be applied once in setup(), so a change over HTTP or MQTT
// did nothing until the next reboot.
void updateHead(bool triggered, unsigned long now) {
  const BassConfig& cfg = config();

  if (triggered) {
    lastSoundTime = now;

    if (!headActive) {
      headActive = true;
      headDriveAt = now;
      headMotor->setSpeed(cfg.headSpeed);
      headMotor->run(FORWARD);
    }
  }

  if (!headActive) return;

  if (now - lastSoundTime >= cfg.headTimeoutMs) {
    headMotor->run(RELEASE);
    headActive = false;
    if (DEBUG) Serial.println("Head released (timeout)");
    return;
  }

  // Drive stroke first, then hold. A hold speed of 0 leaves the motor running
  // at zero duty, so the spring takes the head straight back in - the stay-out
  // window still has to expire before a new stroke can start.
  headMotor->setSpeed(now - headDriveAt < cfg.headMoveMs ? cfg.headSpeed
                                                         : cfg.headHoldSpeed);
}

// Flaps the tail for as long as the fish is talking. A flap is a drive stroke
// followed by a release stroke the return spring completes - the tail swings
// both ways, but the motor is only ever energised forward.
void updateTail(bool speaking) {
  const BassConfig& cfg = config();

  if (!speaking) {
    if (tailFlapping) {
      tailMotor->run(RELEASE);
      tailFlapping = false;
      tailDriving = false;
    }
    return;
  }

  unsigned long now = millis();

  // Backdate the phase clock so the first loop of an utterance starts a drive
  // stroke immediately rather than waiting out a release stroke first.
  if (!tailFlapping) {
    tailFlapping = true;
    tailDriving = false;
    tailPhaseAt = now - cfg.tailFlapMs;
  }

  if (now - tailPhaseAt < cfg.tailFlapMs) return;

  tailPhaseAt = now;
  tailDriving = !tailDriving;

  if (tailDriving) {
    tailMotor->setSpeed(cfg.tailSpeed);
    tailMotor->run(FORWARD);
  } else {
    tailMotor->run(RELEASE);
  }
}

void setup() {
  failsafeBegin();          // must run before anything that can fault
  statusDisplayBegin();
  configBegin(DEFAULT_CONFIG);
  speechBegin(SENSOR_MAP_MAX);   // speak in the same units the ADC maps to
  soundBegin(SOUND_PIN, SENSOR_MAP_MAX);
  telemetrySetScale(SENSOR_MAP_MAX);

  AFMS.begin();

  mouthMotor->setSpeed(0);
  mouthMotor->run(FORWARD);
  mouthMotor->run(RELEASE);

  headMotor->setSpeed(config().headSpeed);
  headMotor->run(FORWARD);
  headMotor->run(RELEASE);

  tailMotor->setSpeed(config().tailSpeed);
  tailMotor->run(FORWARD);
  tailMotor->run(RELEASE);

  pinMode(SOUND_PIN, INPUT);
  if (DEBUG) Serial.begin(9600);

  if (DEBUG) {
    Serial.print("Boot #");
    Serial.print(failsafeBootCount());
    if (swatchdogTripped()) Serial.print(" (watchdog reset)");
    Serial.println();
  }

  if (failsafeSafeMode()) {
    releaseMotors();
    statusDisplay(STATUS_SAFE_MODE);
    if (DEBUG) Serial.println("SAFE MODE: too many failed boots, OTA only");
    delay(2000);            // hold the glyph long enough to be seen
  }

  wifiOtaSetup(releaseMotors);
  mqttBegin();
  swatchdogBegin(WATCHDOG_TIMEOUT_MS);
}

// Brings up the services that need a live link, and services them. Each begin()
// is idempotent, so this also covers WiFi arriving late after a failed boot
// connect.
void serviceNetwork() {
  if (!wifiOtaConnected()) return;

  netlogBegin(LOG_PORT);
  httpApiBegin(HTTP_PORT);
  httpApiLoop();
  mqttLoop();

  static unsigned long lastTelemetry = 0;
  if (millis() - lastTelemetry >= TELEMETRY_INTERVAL_MS) {
    lastTelemetry = millis();
    String line;
    line.reserve(96);
    telemetryFormat(line);
    netlogPrint(line);
  }
}

void loop() {
  swatchdogPet();
  failsafeLoop();
  wifiOtaLoop(!headActive);
  serviceNetwork();

  if (wifiOtaUpdating()) return;
  if (failsafeSafeMode()) return;   // OTA only until a healthy boot clears it

  const BassConfig& cfg = config();
  motorTestLoop();
  speechLoop();
  soundLoop();

  // A manual motor test outranks everything, so a negative result points at
  // wiring or power rather than at the sensing path.
  uint8_t testMotor; int testSpeed;
  if (motorTestActive(testMotor, testSpeed)) {
    if (testMotor != 3) updateTail(false);   // do not leave a flap mid-stroke
    Adafruit_DCMotor* m = (testMotor == 1) ? mouthMotor
                        : (testMotor == 2) ? headMotor
                                           : tailMotor;
    m->setSpeed(testSpeed);
    m->run(FORWARD);
    telemetrySet(0, testMotor == 1 ? testSpeed : 0, testMotor == 2, testMotor == 3,
                 soundRaw(), soundPeakToPeak(), soundRawPeakToPeak(),
                 soundSamplesPerWindow(), soundAverage(), 0, false);
    return;
  }
  if (motorTestConsumeEnded()) releaseMotors();

  // While speaking, the synthesised envelope stands in for the microphone. Every
  // behaviour below - threshold, mouth scaling, head engage and release - runs
  // unchanged, because it cannot tell the difference.
  // soundLevel() measures peak-to-peak swing, so the DC bias on A0 no longer
  // reads as a permanent loud sound.
  bool speaking = speechActive();
  int sensorValue = speaking ? speechLevel() : soundLevel();

  // The tail tracks the utterance, not the sound level - it flaps for as long
  // as the fish is talking and holds still the rest of the time.
  updateTail(speaking);

  // Adaptive threshold. Continuous audio never falls back to silence, so one
  // fixed number cannot suit both a quiet room and a loud track: set it low and
  // the mouth is pinned open, set it high and it never opens. Riding it on the
  // running mean makes the mouth track peaks at whatever the current volume
  // happens to be. It only ever raises the bar - cfg.soundThreshold stays the
  // hard noise gate underneath. Speech is exempt: that envelope is synthesised
  // with real gaps already, and is built around the absolute threshold.
  int threshold = cfg.soundThreshold;
  if (!speaking && cfg.adaptPct > 0) {
    long adaptive = (long)soundAverage() * cfg.adaptPct / 100;
    if (adaptive > threshold) threshold = (int)adaptive;
  }
  // map() below needs a range to work with, so the threshold can never reach
  // the top of the scale.
  if (threshold >= SENSOR_MAP_MAX) threshold = SENSOR_MAP_MAX - 1;

  unsigned long currentMillis = millis();
  int mouthSpeed = 0;

  const bool above = sensorValue > threshold;

  updateHead(above, currentMillis);

  if (cfg.articulate) {
    // Pulsed. Holding the motor on for the length of a phrase never gives the
    // return spring a turn, so the mouth just sits open - which is the whole
    // reason a sustained note or a backing track pinned it. Each rise past the
    // threshold instead fires one bounded pulse, and mouthReadyAt keeps the
    // next one from starting until the spring has had its closed window.
    if (mouthDriving) {
      if ((long)(currentMillis - mouthPulseUntil) >= 0) {
        mouthMotor->run(RELEASE);
        mouthDriving = false;
        mouthReadyAt = currentMillis + cfg.mouthOffMs;
      } else {
        mouthSpeed = mouthPulseSpeed;
      }
    } else if (above && (long)(currentMillis - mouthReadyAt) >= 0) {
      mouthPulseSpeed = map(sensorValue, threshold, SENSOR_MAP_MAX,
                            cfg.mouthSpeedMin, cfg.mouthSpeedMax);
      mouthPulseSpeed = constrain(mouthPulseSpeed, cfg.mouthSpeedMin, cfg.mouthSpeedMax);
      mouthMotor->setSpeed(mouthPulseSpeed);
      mouthMotor->run(FORWARD);
      mouthDriving = true;
      mouthPulseUntil = currentMillis + cfg.mouthOnMs;
      mouthSpeed = mouthPulseSpeed;
    }
  } else if (above) {
    // Plain proportional hold, kept so the two can be compared without a
    // reflash. This is what pins the mouth open on continuous audio.
    mouthSpeed = map(sensorValue, threshold, SENSOR_MAP_MAX, cfg.mouthSpeedMin, cfg.mouthSpeedMax);
    mouthSpeed = constrain(mouthSpeed, cfg.mouthSpeedMin, cfg.mouthSpeedMax);
    mouthMotor->setSpeed(mouthSpeed);
    mouthMotor->run(FORWARD);
  } else {
    mouthMotor->run(RELEASE);
  }

  telemetrySet(sensorValue, mouthSpeed, headActive, tailFlapping,
               soundRaw(), soundPeakToPeak(), soundRawPeakToPeak(),
               soundSamplesPerWindow(), soundAverage(), threshold, speaking);
}
