#include "Arduino.h"
#include "config.h"
#include "voice.h"
#include <cmath>
#include <cstdio>
#include <random>

unsigned long g_millis = 0;

// Stand-in for the real config store. Matches the sketch defaults.
static BassConfig cfg;
const BassConfig& config() { return cfg; }

static const int FS = 4000;          // decimated rate voice.cpp sees
static const int FRAME = 104;        // samples a 30ms window delivers after warm-up
static std::mt19937 rng(1234);

// The path in sound.cpp hands voice.cpp a signal that has already been
// high-passed at 300Hz and low-passed at 900Hz, so the fundamental of a real
// voice is mostly gone and only its middle harmonics survive. Generating the
// same shape here is the point of the test: a detector that only works when the
// fundamental is present would pass a naive sine test and fail on the board.
static double phase = 0.0;
static void harmonics(double f0, double amp, int n, int16_t* out) {
  std::normal_distribution<double> noise(0.0, amp * 0.04);
  for (int i = 0; i < n; i++) {
    double t = phase + (double)i / FS;
    double v = 0.0;
    for (int h = 1; h <= 12; h++) {
      double f = f0 * h;
      if (f < 300.0 || f > 900.0) continue;   // what the tap actually passes
      v += sin(2.0 * M_PI * f * t) / h;
    }
    out[i] = (int16_t)(amp * v + noise(rng));
  }
  phase += (double)n / FS;
}

static void whiteNoise(double amp, int n, int16_t* out) {
  std::normal_distribution<double> d(0.0, amp);
  for (int i = 0; i < n; i++) out[i] = (int16_t)d(rng);
  phase += (double)n / FS;
}

// Two unrelated harmonic complexes at once - a bass note and a guitar note,
// roughly. This is the case a single-pitch estimator cannot resolve, and the
// smoothness feature is what is supposed to catch it.
static void polyphonic(double f1, double f2, double amp, int n, int16_t* out) {
  std::normal_distribution<double> noise(0.0, amp * 0.04);
  for (int i = 0; i < n; i++) {
    double t = phase + (double)i / FS;
    double v = 0.0;
    for (int h = 1; h <= 12; h++) {
      double a = f1 * h, b = f2 * h;
      if (a >= 300.0 && a <= 900.0) v += sin(2.0 * M_PI * a * t) / h;
      if (b >= 300.0 && b <= 900.0) v += sin(2.0 * M_PI * b * t) / h;
    }
    out[i] = (int16_t)(amp * 0.6 * v + noise(rng));
  }
  phase += (double)n / FS;
}

static void feedFrame(const int16_t* s, int n) {
  voiceFrameReset();
  for (int i = 0; i < n; i++) voiceFeed(s[i]);
  voiceAnalyse();
  g_millis += 35;                    // one window plus the rest of loop()
}

// ---- single-frame pitch accuracy ----------------------------------------
static void pitchCase(const char* name, double f0) {
  int16_t s[FRAME];
  phase = 0.0;
  int hits = 0, sum = 0, confSum = 0;
  for (int k = 0; k < 20; k++) {
    harmonics(f0, 300.0, FRAME, s);
    voiceFrameReset();
    for (int i = 0; i < FRAME; i++) voiceFeed(s[i]);
    voiceAnalyse();
    int f = voiceF0();
    confSum += voiceConfidence();
    if (f > 0) { hits++; sum += f; }
  }
  printf("  %-22s want %6.1f Hz   got %6.1f Hz  voiced %2d/20  conf %3d\n",
         name, f0, hits ? (double)sum / hits : 0.0, hits, confSum / 20);
}

// ---- whole-scene scoring -------------------------------------------------
enum Scene { TALK, STARVED, SING, HELD, DRONE, NOISE, MUSIC, BAND, HUM };

static void scene(const char* name, Scene kind, int frames) {
  voiceBegin(FS);
  g_millis = 0;
  phase = 0.0;
  int16_t s[FRAME];
  std::uniform_real_distribution<double> jitter(-1.0, 1.0);
  double f0 = 140.0;
  int held = 0;

  int scoreSum = 0, openFrames = 0, samples = 0;

  for (int k = 0; k < frames; k++) {
    // STARVED is talking with every other window cut to 42 samples - what a
    // 15ms window delivers, or a 30ms one that WiFi took a bite out of. Those
    // windows must be left out of the history, not counted as silence.
    const int len = (kind == STARVED && (k & 1)) ? 42 : FRAME;
    switch (kind) {
      case STARVED:
      case TALK:
        // Voiced runs of a few frames with a gliding pitch, broken by the
        // consonants and word gaps that make speech intermittent.
        if (held > 0) {
          held--;
          f0 *= 1.0 + 0.035 * jitter(rng);
          if (f0 < 95) f0 = 95;
          if (f0 > 260) f0 = 260;
          harmonics(f0, 300.0, FRAME, s);
        } else if (k % 11 < 7) {
          held = 4 + (rng() % 5);
          f0 = 110.0 + (rng() % 90);
          harmonics(f0, 300.0, FRAME, s);
        } else {
          whiteNoise(60.0, FRAME, s);
        }
        break;
      case SING:
        // Held notes with vibrato, stepping to a new note every ~10 frames.
        if (k % 10 == 0) f0 = 180.0 + (rng() % 120);
        harmonics(f0 * (1.0 + 0.02 * sin(k * 0.9)), 300.0, FRAME, s);
        break;
      case HELD:
        // One sustained sung note, vibrato and nothing else. The hardest thing
        // to tell from an instrument, and the thing an over-eager steadiness
        // test rejects first.
        harmonics(196.0 * (1.0 + 0.02 * sin(k * 1.1)), 300.0, FRAME, s);
        break;
      case BAND:
        // The same two pitched lines, but over a band: a snare-ish burst on the
        // backbeat and a broadband floor under everything. This is what a track
        // playing in the room actually looks like to a single-pitch estimator.
        polyphonic(98.0 * (1 + (k / 17) % 2 * 0.19),
                   146.8 + 20.0 * ((k / 9) % 3), 300.0, FRAME, s);
        {
          std::normal_distribution<double> floorN(0.0, (k % 4 == 0) ? 320.0 : 110.0);
          for (int i = 0; i < FRAME; i++) s[i] = (int16_t)(s[i] + floorN(rng));
        }
        break;
      case HUM:
        // Mains hum or a fan's blade tone: periodic, and as steady as a machine.
        harmonics(200.0 * (1.0 + 0.002 * sin(k * 0.3)), 300.0, FRAME, s);
        break;
      case DRONE:
        harmonics(220.0, 300.0, FRAME, s);
        break;
      case NOISE:
        whiteNoise(250.0, FRAME, s);
        break;
      case MUSIC:
        // Bass plus a chord tone, both moving on their own schedule.
        polyphonic(98.0 * (1 + (k / 17) % 2 * 0.19),
                   146.8 + 20.0 * ((k / 9) % 3), 300.0, FRAME, s);
        break;
    }
    feedFrame(s, len);

    if (k >= 32) {                   // let the history fill before scoring
      scoreSum += voiceScore();
      if (voiceOpen()) openFrames++;
      samples++;
    }
  }

  printf("  %-8s  voiced %3d%%  smooth %3d%%  range %3d%%   score %3d   gate open %3d%%\n",
         name, voiceVoicedPct(), voiceSmoothPct(), voiceRangePct(),
         samples ? scoreSum / samples : 0,
         samples ? openFrames * 100 / samples : 0);
}

int main() {
  cfg.voiceGate = 1;
  cfg.voiceConfMin = 55;
  cfg.voiceScoreMin = 55;
  cfg.voiceHoldMs = 1500;

  printf("\nPitch accuracy (fundamental removed - only 300-900Hz harmonics present)\n");
  voiceBegin(FS);
  pitchCase("low male 95Hz",   95.0);
  pitchCase("male 120Hz",      120.0);
  pitchCase("male 150Hz",      150.0);
  pitchCase("female 200Hz",    200.0);
  pitchCase("female 250Hz",    250.0);
  pitchCase("sung 330Hz",      330.0);
  pitchCase("sung 390Hz",      390.0);

  printf("\nScene scoring (score >= %d opens the gate)\n", cfg.voiceScoreMin);
  scene("talking", TALK,  140);
  scene("starved", STARVED, 140);
  scene("singing", SING,  140);
  scene("held",    HELD,  140);
  scene("drone",   DRONE, 140);
  scene("noise",   NOISE, 140);
  scene("music",   MUSIC, 140);
  scene("band",    BAND,  140);
  scene("hum",     HUM,   140);
  printf("\n");
  return 0;
}
