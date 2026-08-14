/*
  UDP log sink. Broadcasts lines to the local subnet so the fish can be watched
  from a laptop without a serial cable:

      nc -ul 9999

  Deliberately fire-and-forget: no connection, no retries, no blocking if
  nobody is listening. Telemetry is rate-limited by the caller rather than sent
  every loop() pass, which at loop rate would flood the network.
*/

#pragma once
#include <Arduino.h>

void netlogBegin(uint16_t port);

// No-op until netlogBegin() has run and WiFi is up.
void netlogPrint(const String& line);
