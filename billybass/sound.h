/*
  Sound level from the A0 input.

  The audio arriving on A0 is AC riding on a DC bias - roughly mid-rail with the
  ADR-0002 conditioning circuit, near ground with a bare sound-sensor module.
  Reading the pin directly therefore reports the bias as a constant loud sound,
  which pins the motors on permanently.

  This measures peak-to-peak swing over a short window instead. Peak-to-peak is
  inherently immune to the DC offset, so no calibration constant is needed and
  the same code works on either input. A window rather than a single sample also
  avoids reading silence when an instantaneous sample lands on a zero crossing.

  The resting level is tracked separately, purely so the UI can show it during
  calibration - the level itself does not depend on it.

  Samples are run through a 300-3000Hz band-pass before the swing is measured,
  so a bass line and a kick drum stop counting as "loud". Music carries
  continuous low-frequency energy, which pins a full-band envelope above any
  fixed threshold and holds the mouth open for the whole song. Set `voice` to 0
  to measure the unfiltered signal instead.
*/

#pragma once
#include <Arduino.h>

// mappedMax is the scale soundLevel() reports on, matching the sketch's
// existing mapped sensor range.
void soundBegin(uint8_t pin, int mappedMax);

// Call every loop(). Samples the pin and closes off a window when one is due.
void soundLoop();

// Latest level, 0..mappedMax, held steady between windows.
int soundLevel();

// Diagnostics for calibration.
int soundRaw();          // most recent raw ADC reading
int soundBaseline();     // tracked resting level
int soundPeakToPeak();   // last window's swing, voice-band filtered when enabled
int soundRawPeakToPeak();// last window's unfiltered swing, to see what the filter removed
int soundAverage();      // slow running mean of the level, for the adaptive threshold
int soundSamplesPerWindow();
