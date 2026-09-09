#include "voice.h"
#include "config.h"

// Search range for the fundamental. 90Hz sits under a low male speaking voice,
// 400Hz over a high sung note. Widening it does not find more voices, it just
// finds periods in things that are not voices.
static const int F0_MIN_HZ = 90;
static const int F0_MAX_HZ = 400;

// YIN's absolute threshold on the normalised difference, scaled by 1000. The
// *first* lag to dip below this wins rather than the deepest one, which is what
// keeps the estimate off the octave below: a true period always has a double
// that dips just as far, and taking the earliest dip picks the real one.
static const int32_t YIN_THRESHOLD = 350;

// How far apart two neighbouring frames' pitches can be and still count as one
// continuous track, as a percentage. A voice gliding through a sentence moves a
// few percent per frame; a detector looking at two instruments at once jumps by
// an octave.
static const int SMOOTH_TOL_PCT = 25;

// Frames of history behind the three features - about 1.1s at a 35ms frame.
// Long enough to hold a couple of syllables, short enough that the gate opens
// within about half a second of someone starting to talk.
static const uint8_t HIST = 32;

// Fewest voiced neighbour-pairs before the smoothness figure means anything.
// Below this it is one or two coincidences, not a track.
static const uint8_t MIN_PAIRS = 4;

// Sample buffer for one window. 160 at 4kHz is 40ms, comfortably over the 30ms
// window sound.cpp actually delivers, so a slow window is never truncated.
static const uint16_t BUF_MAX = 160;

// Longest lag the search can reach, fixed at compile time so the difference
// array can be a plain static. 4000/90 rounds to 44; the +1 is the array's
// inclusive upper index.
static const uint16_t TAU_LIMIT = 48;

static int16_t buf[BUF_MAX];
static uint16_t filled = 0;

// Holds the raw normalised difference on the way in and the cumulative-mean
// normalised difference on the way out - the running mean consumes each entry
// before it is overwritten, so one array does both.
static int32_t diff[TAU_LIMIT + 1];

static int sampleHz = 4000;
static uint16_t tauMin = 10;
static uint16_t tauMax = 44;

static int lastF0 = 0;
static int lastConf = 0;
static uint16_t lastFill = 0;

static int16_t f0Hist[HIST];
static uint8_t histPos = 0;
static bool histFull = false;

static int voicedPct = 0;
static int smoothPct = 0;
static int rangePct = 0;
static int score = 0;
static unsigned long openUntil = 0;

// Piecewise ramp: 0 at or below lo, 100 at or above hi, linear between.
static int rise(int v, int lo, int hi) {
  if (v <= lo) return 0;
  if (v >= hi) return 100;
  return (v - lo) * 100 / (hi - lo);
}

void voiceBegin(int decimatedHz) {
  sampleHz = decimatedHz > 0 ? decimatedHz : 4000;

  // Clamped in this order so the array bound wins over the frequency range: a
  // decimated rate far above what sound.cpp feeds would otherwise widen the
  // search past diff[], and the parabolic step reads one entry past the end.
  tauMin = (uint16_t)(sampleHz / F0_MAX_HZ);
  if (tauMin < 2) tauMin = 2;
  if (tauMin > TAU_LIMIT - 3) tauMin = TAU_LIMIT - 3;

  tauMax = (uint16_t)(sampleHz / F0_MIN_HZ);
  if (tauMax < tauMin + 3) tauMax = tauMin + 3;
  if (tauMax > TAU_LIMIT) tauMax = TAU_LIMIT;

  filled = 0;
  lastFill = 0;
  lastF0 = 0;
  lastConf = 0;
  histPos = 0;
  histFull = false;
  for (uint8_t i = 0; i < HIST; i++) f0Hist[i] = 0;
  voicedPct = smoothPct = rangePct = score = 0;
  openUntil = 0;
}

void voiceFrameReset() {
  filled = 0;
}

void voiceFeed(int16_t sample) {
  if (filled < BUF_MAX) buf[filled++] = sample;
}

// YIN over the current buffer. Sets lastF0 and lastConf. Returns false if the
// window was too short to analyse at all, in which case neither says anything.
static bool estimatePitch() {
  lastF0 = 0;
  lastConf = 0;

  // Two full periods at the longest lag is the shortest window the difference
  // function can be trusted over. A window shorter than that is not evidence
  // of anything - it is a window that WiFi or an HTTP request ate - so it is
  // reported as no result rather than as no voice.
  const uint16_t need = (uint16_t)(2 * tauMax);
  if (filled < need) return false;

  const uint16_t w = filled;

  // Squared difference, divided by the overlap so long and short lags are
  // compared on the same footing - without that the function slopes upward on
  // sample count alone and every estimate lands at the bottom of the range.
  // Computed from lag 1, not from tauMin: the short lags are never candidates,
  // but the normalisation below needs them. Adjacent samples of a band-limited
  // signal are alike, so the difference is small there and the running mean
  // starts small - which is what makes the normalised function large at short
  // lags and lets the first real dip stand out. Started at tauMin instead, the
  // mean was seeded by tauMin's own value, so tauMin normalised to exactly 1.0
  // and could never be chosen: the top of the range was 364Hz, not 400.
  for (uint16_t tau = 1; tau <= tauMax; tau++) {
    const uint16_t n = w - tau;
    int32_t sum = 0;
    for (uint16_t i = 0; i < n; i++) {
      const int32_t d = (int32_t)buf[i] - (int32_t)buf[i + tau];
      sum += d * d;
    }
    diff[tau] = sum / (int32_t)n;
  }

  // Cumulative mean normalisation, in place. running is 64-bit because the sum
  // of ~35 lags of a loud window overflows a signed 32-bit accumulator; the
  // individual entries do not.
  int64_t running = 0;
  for (uint16_t tau = 1; tau <= tauMax; tau++) {
    running += diff[tau];
    const int64_t mean = running / tau;
    int32_t d = (mean > 0) ? (int32_t)(((int64_t)diff[tau] * 1000) / mean) : 1000;
    if (d > 2000) d = 2000;
    diff[tau] = d;
  }

  // First dip under the threshold, then walk down to the bottom of that dip.
  uint16_t best = 0;
  for (uint16_t tau = tauMin; tau <= tauMax; tau++) {
    if (diff[tau] >= YIN_THRESHOLD) continue;
    while (tau < tauMax && diff[tau + 1] < diff[tau]) tau++;
    best = tau;
    break;
  }

  // Nothing cleared the threshold: take the shallowest dip there was, and let
  // the confidence figure say how little it means.
  if (best == 0) {
    best = tauMin;
    for (uint16_t tau = tauMin + 1; tau <= tauMax; tau++) {
      if (diff[tau] < diff[best]) best = tau;
    }
  }

  lastConf = 100 - (int)(diff[best] / 10);
  if (lastConf < 0) lastConf = 0;
  if (lastConf > 100) lastConf = 100;

  // Parabolic interpolation through the three points around the dip. The lag
  // grid is coarse at the top of the range - 10 samples is 400Hz and 11 is
  // 364Hz - so without this the pitch track reports steps a voice never made
  // and the smoothness figure punishes it for them.
  float tauF = (float)best;
  if (best > 1 && best < tauMax) {
    const float a = (float)diff[best - 1];
    const float b = (float)diff[best];
    const float c = (float)diff[best + 1];
    const float denom = a - 2.0f * b + c;
    if (denom != 0.0f) {
      const float shift = 0.5f * (a - c) / denom;
      if (shift > -1.0f && shift < 1.0f) tauF += shift;
    }
  }

  if (tauF > 0.0f) lastF0 = (int)((float)sampleHz / tauF + 0.5f);
  return true;
}

// Recomputes the three history features and the score from f0Hist.
static void scoreHistory() {
  const uint8_t n = histFull ? HIST : histPos;
  if (n == 0) {
    voicedPct = smoothPct = rangePct = score = 0;
    return;
  }

  uint8_t voiced = 0;
  uint8_t pairs = 0;
  uint8_t smooth = 0;
  int minF0 = 0;
  int maxF0 = 0;
  int prev = 0;

  // Walk oldest to newest so "neighbouring frames" really are neighbours in
  // time; the ring's write position is the oldest entry once it has wrapped.
  const uint8_t start = histFull ? histPos : 0;
  for (uint8_t k = 0; k < n; k++) {
    const int f = f0Hist[(uint8_t)((start + k) % HIST)];

    if (f > 0) {
      voiced++;
      if (minF0 == 0 || f < minF0) minF0 = f;
      if (f > maxF0) maxF0 = f;

      if (prev > 0) {
        pairs++;
        const int delta = (f > prev) ? (f - prev) : (prev - f);
        if (delta * 100 <= SMOOTH_TOL_PCT * prev) smooth++;
      }
    }

    prev = f;
  }

  voicedPct = voiced * 100 / n;
  smoothPct = (pairs >= MIN_PAIRS) ? (smooth * 100 / pairs) : 0;
  rangePct  = (voiced >= 2 && minF0 > 0) ? ((maxF0 - minF0) * 100 / minF0) : 0;

  // Voiced share. Under about 12% there is no fundamental to speak of - a fan,
  // applause, a room, a hand clap. There is deliberately no penalty at the top
  // end: singing runs at 100% voiced, and an earlier version that docked it for
  // that rejected every sung phrase. A drone is also 100% voiced, but the range
  // feature below already answers it, and answers it for the right reason.
  const int sVoiced = rise(voicedPct, 12, 30);

  // Track continuity. This is the feature that rejects a full mix: with a bass
  // line, a guitar and a voice all sounding at once the estimate lands on a
  // different one of them every frame.
  const int sSmooth = rise(smoothPct, 25, 55);

  // Pitch travel. A voice cannot hold a pitch still and a machine cannot help
  // it, so this is what separates a talker or a singer from a fan, a mains hum
  // or a synth pad - all of which are perfectly periodic and perfectly steady.
  // The knee is low because a held sung note only has its vibrato to offer,
  // which is a couple of percent; anything under about 1% is not a larynx.
  const int sRange = rise(rangePct, 1, 6);

  int weakest = sVoiced;
  if (sSmooth < weakest) weakest = sSmooth;
  if (sRange  < weakest) weakest = sRange;
  score = weakest;
}

void voiceAnalyse() {
  lastFill = filled;

  // A starved window is left out of the history entirely. Pushing it as
  // unvoiced dragged the voiced share down every time the network took a bite
  // out of the sampling loop - which is exactly when someone has the web UI
  // open watching the score. The gate holds whatever the last real frame said.
  if (!estimatePitch()) return;

  const BassConfig& cfg = config();
  const bool voicedFrame = (lastF0 > 0) && (lastConf >= cfg.voiceConfMin);

  f0Hist[histPos] = voicedFrame ? (int16_t)lastF0 : 0;
  histPos = (uint8_t)((histPos + 1) % HIST);
  if (histPos == 0) histFull = true;

  scoreHistory();

  if (score >= cfg.voiceScoreMin) openUntil = millis() + cfg.voiceHoldMs;

  // f0 is reported for the frame only if that frame was voiced, so /status
  // shows the pitch track the history is actually built from rather than the
  // detector's best guess at a room full of nothing.
  if (!voicedFrame) lastF0 = 0;
}

int  voiceF0()         { return lastF0; }
int  voiceConfidence() { return lastConf; }
int  voiceVoicedPct()  { return voicedPct; }
int  voiceSmoothPct()  { return smoothPct; }
int  voiceRangePct()   { return rangePct; }
int  voiceScore()      { return score; }
int  voiceSamples()    { return lastFill; }

bool voiceOpen() {
  // Subtraction so this survives the millis() rollover.
  return (long)(millis() - openUntil) < 0;
}
