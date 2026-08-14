#include "telemetry.h"

static Telemetry latest = { 0, 0, false, 0, 0, false };
static int scale = 180;

void telemetrySet(int soundLevel, int mouthSpeed, bool headActive,
                  int raw, int peakToPeak, bool speaking) {
  latest.soundLevel = soundLevel;
  latest.mouthSpeed = mouthSpeed;
  latest.headActive = headActive;
  latest.raw = raw;
  latest.peakToPeak = peakToPeak;
  latest.speaking = speaking;
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
  dst += " raw=";   dst += latest.raw;
  dst += " pp=";    dst += latest.peakToPeak;
  dst += " speaking="; dst += latest.speaking ? 1 : 0;
  dst += " scale="; dst += scale;
  dst += " uptime="; dst += (millis() / 1000); dst += 's';
}
