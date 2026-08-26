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
  bool tailActive;     // flapping while speaking
  int raw;             // last raw ADC count, for calibration
  int peakToPeak;      // last window's swing, voice-band filtered when enabled
  int rawPeakToPeak;   // same window unfiltered, to see what the filter removed
  int samples;         // readings taken in that window - 1 means the window is starved
  int average;         // running mean of the level
  int threshold;       // level the mouth actually had to clear this pass
  bool speaking;       // synthesised envelope is driving, not the ADC

  // Button. Reported raw and uninterpreted - nothing acts on it yet, so this
  // is how you confirm it is actually wired before writing anything that
  // depends on it.
  bool button;            // debounced, true while held
  uint16_t presses;       // button presses since boot
};

void telemetrySet(int soundLevel, int mouthSpeed, bool headActive, bool tailActive,
                  int raw, int peakToPeak, int rawPeakToPeak, int samples,
                  int average, int threshold, bool speaking);

// Separate from telemetrySet() because the button is sampled before the paths
// that can return early - a motor test, safe mode - and so stays live on
// /status when the sound and motor fields do not.
void telemetrySetInputs(bool button, uint16_t presses);

const Telemetry& telemetry();

// Full-scale sound level, so consumers can render a meter without hardcoding
// the sketch's mapped sensor range.
void telemetrySetScale(int fullScale);
int telemetryScale();

// Appends "sound=.. mouth=.. head=.. tail=.. raw=.. pp=.. ppraw=.. samples=..
// avg=.. thr=.. speaking=.. scale=.. uptime=..s btn=.. btnn=..".
void telemetryFormat(String& dst);
