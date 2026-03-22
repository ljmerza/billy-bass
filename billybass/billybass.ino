/*
  Big Mouth Billy Bass - Sound Reactive Animatronic
  Originally by Donald Bell, Maker Project Lab (2016).
  Based on Sound to Servo by Cenk Ozdemir (2012)
  and DCMotorTest by Adafruit
*/

#include <Wire.h>
#include <Adafruit_MotorShield.h>
#include "utility/Adafruit_MS_PWMServoDriver.h"

// ----- Pin Configuration -----
const int SOUND_PIN = A0;               // Analog pin for sound sensor input

// ----- Motor Configuration -----
const int MOUTH_MOTOR_PORT = 1;         // Motor shield port for mouth
const int HEAD_MOTOR_PORT  = 2;         // Motor shield port for head
const int HEAD_SPEED       = 254;       // Head motor speed (0-255)
const int MOUTH_SPEED_MIN  = 140;       // Mouth motor ramp start speed
const int MOUTH_SPEED_MAX  = 254;       // Mouth motor ramp end speed

// ----- Sound Detection -----
const int SOUND_THRESHOLD  = 30;        // Minimum mapped value to trigger motors
const int SENSOR_RAW_MAX   = 512;       // Upper range of raw analog reading
const int SENSOR_MAP_MAX   = 180;       // Upper range after mapping

// ----- Timing -----
const unsigned long HEAD_TIMEOUT_MS = 3000;  // Silence duration before head releases

// ----- Globals -----
Adafruit_MotorShield AFMS = Adafruit_MotorShield();
Adafruit_DCMotor *mouthMotor = AFMS.getMotor(MOUTH_MOTOR_PORT);
Adafruit_DCMotor *headMotor  = AFMS.getMotor(HEAD_MOTOR_PORT);
unsigned long lastSoundTime = 0;

void setup() {
  AFMS.begin();

  mouthMotor->setSpeed(0);
  mouthMotor->run(FORWARD);
  mouthMotor->run(RELEASE);

  headMotor->setSpeed(HEAD_SPEED);
  headMotor->run(FORWARD);
  headMotor->run(RELEASE);

  pinMode(SOUND_PIN, INPUT);
  Serial.begin(9600);
}

void loop() {
  int sensorValue = analogRead(SOUND_PIN);
  sensorValue = map(sensorValue, 0, SENSOR_RAW_MAX, 0, SENSOR_MAP_MAX);

  unsigned long currentMillis = millis();

  if (sensorValue > SOUND_THRESHOLD) {
    lastSoundTime = currentMillis;
    headMotor->run(FORWARD);
    mouthMotor->run(FORWARD);

    for (uint8_t i = MOUTH_SPEED_MIN; i < MOUTH_SPEED_MAX + 1; i++) {
      mouthMotor->setSpeed(i);
    }

    mouthMotor->run(RELEASE);
  }

  if (currentMillis - lastSoundTime >= HEAD_TIMEOUT_MS) {
    headMotor->run(RELEASE);
  }
}
