/*
  OTA failsafe. Counts consecutive boots that never reached a healthy state and
  drops into an OTA-only safe mode once too many pile up.

  The risk this covers: push a broken sketch over WiFi and you normally lose OTA
  along with it, which means opening the enclosure and reaching for a USB cable.
  If the fish reboots repeatedly without ever running long enough to be declared
  healthy, safe mode skips the motor logic entirely and runs nothing but WiFi
  and the OTA server, leaving a way back in.

  Limits worth knowing: this cannot help if the sketch faults before
  failsafeBegin() runs, or if the fault is inside WiFi bring-up itself.
*/

#pragma once
#include <Arduino.h>

// Reads and increments the consecutive-failed-boot counter. Call as the very
// first statement in setup(), before any hardware is touched.
void failsafeBegin();

// True when the sketch should skip normal operation and run OTA only.
bool failsafeSafeMode();

// Consecutive boots that never reached healthy, as of this boot.
unsigned int failsafeBootCount();

// Call every loop(). Once the sketch has run without resetting for long enough,
// this clears the counter - one flash write per boot, not per call.
void failsafeLoop();
