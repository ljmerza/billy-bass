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
const int HEAD_SPEED       = 254;       // Head motor speed (0-255)
const int MOUTH_SPEED_MIN  = 140;       // Minimum mouth motor speed when sound detected
const int MOUTH_SPEED_MAX  = 254;       // Maximum mouth motor speed at loudest sound

// ----- Sound Detection -----
const int SOUND_THRESHOLD  = 30;        // Minimum mapped value to trigger motors
const int SENSOR_RAW_MAX   = 512;       // Upper range of raw analog reading
const int SENSOR_MAP_MAX   = 180;       // Upper range after mapping

// ----- Timing -----
const unsigned long HEAD_TIMEOUT_MS = 3000;  // Silence duration before head releases

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
  SOUND_THRESHOLD, MOUTH_SPEED_MIN, MOUTH_SPEED_MAX, HEAD_SPEED, HEAD_TIMEOUT_MS,
  0,      // speakDelayMs - raise to line the mouth up with audio played elsewhere
  100,    // speakRatePct - 100 is normal speaking pace
  300     // ppFullScale - raw ADC swing treated as full volume
};

// ----- Globals -----
Adafruit_MotorShield AFMS = Adafruit_MotorShield();
Adafruit_DCMotor *mouthMotor = AFMS.getMotor(MOUTH_MOTOR_PORT);
Adafruit_DCMotor *headMotor  = AFMS.getMotor(HEAD_MOTOR_PORT);
unsigned long lastSoundTime = 0;
bool headActive = false;

// Stops both motors. Passed to wifiOtaSetup(), which calls it before anything
// that stalls or resets the sketch - an OTA transfer, the flash copy before the
// reboot, or a blocking WiFi connect attempt. The motor shield keeps its PWM
// registers across an MCU reset, so releasing here is what holds the fish still
// until setup() runs AFMS.begin() again.
void releaseMotors() {
  speechStop();             // abandon any utterance rather than resume mid-word
  mouthMotor->run(RELEASE);
  headMotor->run(RELEASE);
  headActive = false;
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
  uint8_t testMotor; int testSpeed; int testDir;
  if (motorTestActive(testMotor, testSpeed, testDir)) {
    Adafruit_DCMotor* m = (testMotor == 1) ? mouthMotor : headMotor;
    m->setSpeed(testSpeed);
    m->run(testDir < 0 ? BACKWARD : FORWARD);
    telemetrySet(0, testMotor == 1 ? testSpeed : 0, testMotor == 2,
                 soundRaw(), soundPeakToPeak(), false);
    return;
  }
  if (motorTestConsumeEnded()) releaseMotors();

  // While speaking, the synthesised envelope stands in for the microphone. Every
  // behaviour below - threshold, mouth scaling, head engage and release - runs
  // unchanged, because it cannot tell the difference.
  // soundLevel() measures peak-to-peak swing, so the DC bias on A0 no longer
  // reads as a permanent loud sound.
  int sensorValue = speechActive() ? speechLevel() : soundLevel();

  unsigned long currentMillis = millis();
  int mouthSpeed = 0;

  if (sensorValue > cfg.soundThreshold) {
    lastSoundTime = currentMillis;

    if (!headActive) {
      headMotor->run(FORWARD);
      headActive = true;
    }

    // Scale mouth speed proportionally to sound level
    mouthSpeed = map(sensorValue, cfg.soundThreshold, SENSOR_MAP_MAX, cfg.mouthSpeedMin, cfg.mouthSpeedMax);
    mouthSpeed = constrain(mouthSpeed, cfg.mouthSpeedMin, cfg.mouthSpeedMax);
    mouthMotor->setSpeed(mouthSpeed);
    mouthMotor->run(FORWARD);

    if (DEBUG) {
      Serial.print("Sound: ");
      Serial.print(sensorValue);
      Serial.print(" | Mouth speed: ");
      Serial.println(mouthSpeed);
    }
  } else {
    mouthMotor->run(RELEASE);
  }

  if (headActive && currentMillis - lastSoundTime >= cfg.headTimeoutMs) {
    headMotor->run(RELEASE);
    headActive = false;
    if (DEBUG) Serial.println("Head released (timeout)");
  }

  telemetrySet(sensorValue, mouthSpeed, headActive,
               soundRaw(), soundPeakToPeak(), speechActive());
}
