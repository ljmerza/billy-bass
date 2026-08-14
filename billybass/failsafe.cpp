#include <Preferences.h>
#include "failsafe.h"

static const char* NAMESPACE = "billybass";
static const char* KEY_BOOTS = "failboots";

// Consecutive unhealthy boots tolerated before dropping to OTA-only.
static const unsigned int SAFE_MODE_THRESHOLD = 3;

// How long the sketch must run before this boot counts as healthy. Long enough
// that a crash loop cannot sneak past it, short enough to clear on a good boot.
static const unsigned long HEALTHY_AFTER_MS = 20000;

static Preferences prefs;
static unsigned int bootCount = 0;
static bool safeMode = false;
static bool healthyWritten = false;

void failsafeBegin() {
  if (!prefs.begin(NAMESPACE, false)) return;

  bootCount = prefs.getUInt(KEY_BOOTS, 0) + 1;
  prefs.putUInt(KEY_BOOTS, bootCount);
  prefs.end();

  safeMode = (bootCount > SAFE_MODE_THRESHOLD);
}

bool failsafeSafeMode() {
  return safeMode;
}

unsigned int failsafeBootCount() {
  return bootCount;
}

void failsafeLoop() {
  if (healthyWritten || millis() < HEALTHY_AFTER_MS) return;

  healthyWritten = true;                    // one flash write per boot, not per call
  if (prefs.begin(NAMESPACE, false)) {
    prefs.putUInt(KEY_BOOTS, 0);
    prefs.end();
  }
}
