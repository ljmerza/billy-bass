#include "sound.h"
#include "config.h"

// Window length. At the 300Hz low end of the voice band this still spans about
// nine cycles, so a quiet passage cannot hide between samples.
static const unsigned long WINDOW_MS = 30;

// How fast the resting level chases the signal midpoint, as a shift. Slow, so
// it settles on the DC bias rather than following the audio.
static const uint8_t BASELINE_SHIFT = 4;

static uint8_t adcPin = A0;
static int fullScale = 180;

static unsigned long windowStart = 0;
static int windowMin = 32767;
static int windowMax = -32768;
static uint16_t windowSamples = 0;

static int lastRaw = 0;
static int baseline = -1;
static int lastPp = 0;
static int lastSamples = 0;
static int level = 0;

void soundBegin(uint8_t pin, int mappedMax) {
  adcPin = pin;
  fullScale = mappedMax;
  pinMode(pin, INPUT);

  windowStart = millis();
  windowMin = 32767;
  windowMax = -32768;
  windowSamples = 0;
  baseline = -1;
  level = 0;
}

void soundLoop() {
  lastRaw = analogRead(adcPin);

  if (lastRaw < windowMin) windowMin = lastRaw;
  if (lastRaw > windowMax) windowMax = lastRaw;
  if (windowSamples < 65535) windowSamples++;

  if (millis() - windowStart < WINDOW_MS) return;

  if (windowSamples > 0) {
    lastPp = windowMax - windowMin;
    lastSamples = windowSamples;

    // Midpoint of the swing is the DC bias. Seed it on the first window so the
    // display is meaningful immediately instead of ramping from zero.
    int mid = (windowMax + windowMin) / 2;
    baseline = (baseline < 0) ? mid : baseline + ((mid - baseline) >> BASELINE_SHIFT);

    long scaled = (long)lastPp * fullScale / config().ppFullScale;
    level = (int)constrain(scaled, 0, (long)fullScale);
  }

  windowStart = millis();
  windowMin = 32767;
  windowMax = -32768;
  windowSamples = 0;
}

int soundLevel() { return level; }
int soundRaw() { return lastRaw; }
int soundBaseline() { return baseline < 0 ? 0 : baseline; }
int soundPeakToPeak() { return lastPp; }
int soundSamplesPerWindow() { return lastSamples; }
