/*
  Manual motor override, for checking M1, M2 and M3 independently.

  Takes priority over both the sound input and text-to-speech, so a test result
  is unambiguous: if the motor does not move here, the problem is wiring, the
  shield, or power - not the sensing logic.

  Forward only. Every mechanism in the fish is spring-returned against a hard
  stop, so driving a motor backward stalls it into that stop. There is no
  direction argument to get wrong.
*/

#pragma once
#include <Arduino.h>

// motor is 1 (mouth), 2 (head) or 3 (tail).
// Bounded duration; anything over MOTOR_TEST_MAX_MS is clamped so a stuck
// request cannot leave a motor driven indefinitely.
static const unsigned long MOTOR_TEST_MAX_MS = 5000;

bool motorTestRequest(uint8_t motor, int speed, unsigned long ms);

// Call every loop() before reading the result.
void motorTestLoop();

// True while a test is running; fills in which motor and how fast to drive it.
bool motorTestActive(uint8_t& motor, int& speed);

// Returns true exactly once, on the loop where a test finishes, so the caller
// knows to release the motors.
bool motorTestConsumeEnded();
