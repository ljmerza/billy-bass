#include "motortest.h"

static uint8_t testMotor = 0;
static int testSpeed = 0;
static unsigned long testUntil = 0;
static bool running = false;
static bool endedFlag = false;

bool motorTestRequest(uint8_t motor, int speed, unsigned long ms) {
  if (motor < 1 || motor > 3) return false;
  if (speed < 0 || speed > 255) return false;
  if (ms == 0) return false;
  if (ms > MOTOR_TEST_MAX_MS) ms = MOTOR_TEST_MAX_MS;

  testMotor = motor;
  testSpeed = speed;
  testUntil = millis() + ms;
  running = true;
  return true;
}

void motorTestLoop() {
  if (!running) return;

  if ((long)(millis() - testUntil) >= 0) {
    running = false;
    endedFlag = true;
  }
}

bool motorTestActive(uint8_t& motor, int& speed) {
  if (!running) return false;

  motor = testMotor;
  speed = testSpeed;
  return true;
}

bool motorTestConsumeEnded() {
  bool e = endedFlag;
  endedFlag = false;
  return e;
}
