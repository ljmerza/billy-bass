/*
  Latest snapshot of what the fish is doing. Written once per loop() by the
  sketch, read by the HTTP, MQTT and UDP-log adapters so none of them has to
  reach into the sketch's globals.
*/

#pragma once
#include <Arduino.h>

struct Telemetry {
  int soundLevel;      // mapped sensor reading
  int mouthSpeed;      // 0 when released
  bool headActive;
  int raw;             // last raw ADC count, for calibration
  int peakToPeak;      // last window's raw swing
  bool speaking;       // synthesised envelope is driving, not the ADC
};

void telemetrySet(int soundLevel, int mouthSpeed, bool headActive,
                  int raw, int peakToPeak, bool speaking);
const Telemetry& telemetry();

// Full-scale sound level, so consumers can render a meter without hardcoding
// the sketch's mapped sensor range.
void telemetrySetScale(int fullScale);
int telemetryScale();

// Appends "sound=.. mouth=.. head=.. raw=.. pp=.. speaking=.. scale=.. uptime=..s".
void telemetryFormat(String& dst);
