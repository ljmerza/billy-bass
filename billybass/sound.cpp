#include "sound.h"
#include "config.h"
#include "voice.h"

// Window length. Also the loop period, because the whole window is sampled in
// one blocking pass - so this sets how promptly the mouth pulse in the sketch
// can be ended. At 30ms a 45ms pulse ran anywhere from 45 to 80ms and the mouth
// looked sluggish; 15ms halves that overrun. The cost is at the bottom of the
// voice band, where the window now spans about four and a half cycles at 300Hz
// rather than nine, so the peak-to-peak reading is noisier there.
static const unsigned long WINDOW_MS = 15;

// Sampling is paced to a fixed rate rather than run flat out. The filter
// coefficients are derived for SAMPLE_HZ, so a free-running analogRead() - whose
// rate moves with whatever else the core is doing - would drag the corner
// frequencies around with it. 16kHz leaves plenty of headroom over the 3kHz top
// of the voice band, and analogRead() on this core measures near 43kHz, so the
// pacing spin has time to spare.
static const unsigned long SAMPLE_HZ = 16000;
static const unsigned long SAMPLE_INTERVAL_US = 1000000UL / SAMPLE_HZ;

// Voice band. Two one-pole stages at the bottom (12dB/octave) because that is
// where the problem is - a kick drum at 60Hz lands about 28dB down instead of
// the 14dB a single stage would give. One stage at the top is enough to take
// the hiss off.
static const float HP_HZ = 300.0f;
static const float LP_HZ = 3000.0f;

// Sampling stops between windows, so the filter sees a step when it resumes and
// rings for a moment. These samples are pushed through the filter but kept out
// of the min/max, letting it settle first. 64 samples is 4ms, well over the
// 3.3ms time constant of the 300Hz stage.
static const uint16_t WARMUP_SAMPLES = 64;

// How fast the resting level chases the signal midpoint, as a shift. Slow, so
// it settles on the DC bias rather than following the audio.
static const uint8_t BASELINE_SHIFT = 4;

// Running mean of the level, as a shift. About 32 windows - half a second at
// the 15ms window above - so it sits above syllable rate but still follows a
// volume change within a bar or two of music.
static const uint8_t AVERAGE_SHIFT = 5;

// Pitch tap. voice.cpp needs a fundamental to measure, and the 300-3000Hz chain
// above is the wrong shape to hand it one: it exists to measure loudness in the
// band, and by the second high-pass stage there is little left below 200Hz to
// find a period in. So the tap branches off after the *first* high-pass stage,
// where a kick drum is already 14dB down but the low harmonics of a voice
// survive, and low-passes at 900Hz so the signal can be decimated 4:1 to 4kHz
// without folding. Two low-pass stages, because one is not enough rejection at
// the new 2kHz Nyquist.
static const float PITCH_LP_HZ = 900.0f;
static const uint8_t PITCH_DECIMATE = 4;

static uint8_t adcPin = A0;
static int fullScale = 180;

static int lastRaw = 0;
static int baseline = -1;
static int lastPp = 0;
static int lastSamples = 0;
static int level = 0;

// Fixed-point accumulator holding average << AVERAGE_SHIFT. Kept scaled rather
// than shifting a difference each window, because `(diff) >> n` truncates
// towards negative infinity and would walk the mean downwards on its own.
static long avgAccum = -1;
static int lastRawPp = 0;

// Filter coefficients, derived in soundBegin() from the constants above.
static float hpA = 0.0f;
static float lpAlpha = 0.0f;

// Filter state, carried across windows.
static float hp1x = 0.0f, hp1y = 0.0f;
static float hp2x = 0.0f, hp2y = 0.0f;
static float lpY = 0.0f;

// Pitch tap state, likewise carried across windows.
static float pitchAlpha = 0.0f;
static float pitchLp1 = 0.0f, pitchLp2 = 0.0f;
static float pitchAccum = 0.0f;
static uint8_t pitchCount = 0;

void soundBegin(uint8_t pin, int mappedMax) {
  adcPin = pin;
  fullScale = mappedMax;
  pinMode(pin, INPUT);

  baseline = -1;
  level = 0;
  avgAccum = -1;

  // Standard one-pole RC forms. dt is the sample interval, RC the time constant
  // of the corner frequency.
  const float dt = 1.0f / (float)SAMPLE_HZ;
  const float rcHp = 1.0f / (2.0f * PI * HP_HZ);
  const float rcLp = 1.0f / (2.0f * PI * LP_HZ);
  const float rcPitch = 1.0f / (2.0f * PI * PITCH_LP_HZ);
  hpA = rcHp / (rcHp + dt);
  lpAlpha = dt / (rcLp + dt);
  pitchAlpha = dt / (rcPitch + dt);

  hp1x = hp1y = hp2x = hp2y = lpY = 0.0f;
  pitchLp1 = pitchLp2 = pitchAccum = 0.0f;
  pitchCount = 0;

  voiceBegin((int)(SAMPLE_HZ / PITCH_DECIMATE));
}

void soundLoop() {
  // The whole window is sampled here, in a tight loop. Taking one reading per
  // loop() pass instead does not work: once WiFi, HTTP, MQTT and the OTA poll
  // are in the path a single pass takes longer than WINDOW_MS, so every window
  // closed holding exactly one sample - and with min == max that is a
  // peak-to-peak of zero no matter how loud the audio actually is.
  unsigned long start = millis();
  unsigned long nextSample = micros();
  int windowMin = 32767;
  int windowMax = -32768;
  float bandMin = 0.0f;
  float bandMax = 0.0f;
  bool bandSeeded = false;
  uint16_t samples = 0;

  voiceFrameReset();
  pitchAccum = 0.0f;
  pitchCount = 0;

  // do/while, so a window always holds at least one reading even if millis()
  // ticks over between the two calls.
  do {
    // Spin to the next slot rather than delayMicroseconds(), so a sample that
    // runs late does not push every following one late as well.
    while ((long)(micros() - nextSample) < 0) { }
    nextSample += SAMPLE_INTERVAL_US;

    lastRaw = analogRead(adcPin);
    if (lastRaw < windowMin) windowMin = lastRaw;
    if (lastRaw > windowMax) windowMax = lastRaw;

    const float x = (float)lastRaw;

    const float h1 = hpA * (hp1y + x - hp1x);
    hp1x = x;  hp1y = h1;

    // Pitch tap, off the first high-pass stage - see PITCH_LP_HZ above.
    pitchLp1 += pitchAlpha * (h1 - pitchLp1);
    pitchLp2 += pitchAlpha * (pitchLp1 - pitchLp2);
    pitchAccum += pitchLp2;
    if (++pitchCount >= PITCH_DECIMATE) {
      // Averaging the group rather than keeping one sample in four is a free
      // extra anti-alias stage: a four-tap box has a null right at the 4kHz
      // rate the tap is being decimated to. Warm-up samples are dropped here
      // for the same reason they are kept out of the min/max below.
      if (samples >= WARMUP_SAMPLES) {
        voiceFeed((int16_t)(pitchAccum / (float)PITCH_DECIMATE));
      }
      pitchAccum = 0.0f;
      pitchCount = 0;
    }

    const float h2 = hpA * (hp2y + h1 - hp2x);
    hp2x = h1; hp2y = h2;

    lpY += lpAlpha * (h2 - lpY);

    if (samples >= WARMUP_SAMPLES) {
      if (!bandSeeded) { bandMin = bandMax = lpY; bandSeeded = true; }
      else if (lpY < bandMin) bandMin = lpY;
      else if (lpY > bandMax) bandMax = lpY;
    }

    if (samples < 65535) samples++;
  } while (millis() - start < WINDOW_MS);

  voiceAnalyse();

  lastRawPp = windowMax - windowMin;

  // Fall back to the unfiltered swing if the window was too short to get past
  // warm-up, so a starved window reports something rather than a flat zero.
  const int bandPp = bandSeeded ? (int)(bandMax - bandMin) : lastRawPp;
  lastPp = config().voiceFilter ? bandPp : lastRawPp;
  lastSamples = samples;

  // Midpoint of the swing is the DC bias. Seed it on the first window so the
  // display is meaningful immediately instead of ramping from zero.
  int mid = (windowMax + windowMin) / 2;
  baseline = (baseline < 0) ? mid : baseline + ((mid - baseline) >> BASELINE_SHIFT);

  long scaled = (long)lastPp * fullScale / config().ppFullScale;
  level = (int)constrain(scaled, 0, (long)fullScale);

  // Seed on the first window so the mouth is not gated by a mean of zero that
  // takes a second to catch up.
  if (avgAccum < 0) avgAccum = (long)level << AVERAGE_SHIFT;
  else avgAccum += (long)level - (avgAccum >> AVERAGE_SHIFT);
}

int soundLevel() { return level; }
int soundRaw() { return lastRaw; }
int soundBaseline() { return baseline < 0 ? 0 : baseline; }
int soundPeakToPeak() { return lastPp; }
int soundRawPeakToPeak() { return lastRawPp; }
int soundAverage() { return avgAccum < 0 ? 0 : (int)(avgAccum >> AVERAGE_SHIFT); }
int soundSamplesPerWindow() { return lastSamples; }
