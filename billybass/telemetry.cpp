#include "telemetry.h"

// Positional aggregate initializer - it has to track the field order in
// Telemetry, so new fields go on the end of the struct and on the end here.
static Telemetry latest = { 0, 0, false, false, 0, 0, 0, 0, 0, 0, false,
                            false, 0,
                            0, 0, 0, 0, 0, 0, false, 0 };
static int scale = 180;

void telemetrySet(int soundLevel, int mouthSpeed, bool headActive, bool tailActive,
                  int raw, int peakToPeak, int rawPeakToPeak, int samples,
                  int average, int threshold, bool speaking) {
  latest.soundLevel = soundLevel;
  latest.mouthSpeed = mouthSpeed;
  latest.headActive = headActive;
  latest.tailActive = tailActive;
  latest.raw = raw;
  latest.peakToPeak = peakToPeak;
  latest.rawPeakToPeak = rawPeakToPeak;
  latest.samples = samples;
  latest.average = average;
  latest.threshold = threshold;
  latest.speaking = speaking;
}

void telemetrySetVoice(int f0, int conf, int voicedPct, int smoothPct,
                       int rangePct, int score, bool open, int fill) {
  latest.f0 = f0;
  latest.voiceConf = conf;
  latest.voicedPct = voicedPct;
  latest.voiceSmoothPct = smoothPct;
  latest.voiceRangePct = rangePct;
  latest.voiceScore = score;
  latest.voiceOpen = open;
  latest.voiceFill = fill;
}

void telemetrySetInputs(bool button, uint16_t presses) {
  latest.button = button;
  latest.presses = presses;
}

const Telemetry& telemetry() {
  return latest;
}

void telemetrySetScale(int fullScale) {
  if (fullScale > 0) scale = fullScale;
}

int telemetryScale() {
  return scale;
}

void telemetryFormat(String& dst) {
  dst += "sound=";  dst += latest.soundLevel;
  dst += " mouth="; dst += latest.mouthSpeed;
  dst += " head=";  dst += latest.headActive ? 1 : 0;
  dst += " tail=";  dst += latest.tailActive ? 1 : 0;
  dst += " raw=";   dst += latest.raw;
  dst += " pp=";    dst += latest.peakToPeak;
  dst += " ppraw="; dst += latest.rawPeakToPeak;
  dst += " samples="; dst += latest.samples;
  dst += " avg=";   dst += latest.average;
  dst += " thr=";   dst += latest.threshold;
  dst += " speaking="; dst += latest.speaking ? 1 : 0;
  dst += " scale="; dst += scale;
  dst += " uptime="; dst += (millis() / 1000); dst += 's';
  dst += " btn=";   dst += latest.button ? 1 : 0;
  dst += " btnn=";  dst += latest.presses;
  dst += " f0=";      dst += latest.f0;
  dst += " vconf=";   dst += latest.voiceConf;
  dst += " vpct=";    dst += latest.voicedPct;
  dst += " vsmooth="; dst += latest.voiceSmoothPct;
  dst += " vrange=";  dst += latest.voiceRangePct;
  dst += " vscore=";  dst += latest.voiceScore;
  dst += " vopen=";   dst += latest.voiceOpen ? 1 : 0;
  dst += " vfill=";   dst += latest.voiceFill;
}
