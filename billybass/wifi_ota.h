/*
  WiFi connection and over-the-air firmware updates for the UNO R4 WiFi.
  See docs/adr/0003-wifi-ota-debug.md.
*/

#pragma once
#include <Arduino.h>

// Defined in billybass.ino. Declared here so wifi_ota.cpp can share the same
// debug gate - a file-scope `const` has internal linkage unless a prior extern
// declaration is visible, and billybass.ino includes this header above it.
extern const bool DEBUG;

// Connects to WiFi and starts the OTA service. onHalt is called whenever the
// sketch is about to stop running normally - an upload starting, the flash copy
// before the reset, a failed transfer, or a blocking connect attempt. It must
// leave every actuator de-energised and clear any "is running" state.
void wifiOtaSetup(void (*onHalt)());

// Call every loop(). safeToBlock gates reconnect attempts: WiFiS3 has no
// non-blocking connect, so WiFi.begin() busy-waits for seconds.
void wifiOtaLoop(bool safeToBlock);

// True from the moment an upload starts until the board resets.
bool wifiOtaUpdating();

// True while the WiFi link is up.
bool wifiOtaConnected();
