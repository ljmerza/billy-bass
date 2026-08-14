/*
  Status on the UNO R4 WiFi's built-in 12x8 LED matrix.

  Once the fish is mounted and the enclosure is closed, serial is not visible.
  This is the only feedback channel left. Uses loadFrame() with packed bitmaps
  rather than the text API, which would pull in ArduinoGraphics and its flash
  cost for the sake of scrolling a string.
*/

#pragma once
#include <Arduino.h>

enum StatusState {
  STATUS_BOOTING,
  STATUS_CONNECTING,
  STATUS_ONLINE,
  STATUS_OFFLINE,
  STATUS_OTA,
  STATUS_SAFE_MODE
};

void statusDisplayBegin();

// Idempotent - redrawing the current state costs nothing.
void statusDisplay(StatusState state);

// Blinks the last octet of the IP as that many dots, once, so the address is
// readable without a serial cable. Blocking, ~2s. Call only at boot.
void statusDisplayShowOctet(uint8_t octet);
