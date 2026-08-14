/*
  Software watchdog. A periodic timer interrupt resets the board if loop() has
  not checked in for the configured timeout.

  Chosen over the RA4M1 hardware WDT because that peripheral caps out at about
  5.6 seconds and cannot be stopped once started - it would reset the board
  mid-OTA-upload, since ArduinoOTA.poll() blocks for the whole transfer. This
  version can be suspended for exactly that window.
*/

#pragma once
#include <Arduino.h>

// Starts the supervisor. timeoutMs is how long loop() may go without petting
// before the board is reset. Safe to call with no timer available - it simply
// does nothing and reports false.
bool swatchdogBegin(unsigned long timeoutMs);

// Call once per loop() iteration.
void swatchdogPet();

// Suspend/resume around known-long blocking work (an OTA transfer). Suspend is
// reference-free: a single resume re-arms it, and resume also pets.
void swatchdogSuspend();
void swatchdogResume();

// True if the previous boot was ended by this watchdog rather than by power-on
// or an OTA reset. Only meaningful after swatchdogBegin().
bool swatchdogTripped();
