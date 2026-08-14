#include <Preferences.h>
#include "config.h"

static const char* NAMESPACE = "billybass";

static BassConfig cfg;
static BassConfig defaults;
static bool dirty = false;

// Key names are also the HTTP query params and the MQTT config fields. Keep
// them short - Preferences keys on this core are length-limited.
static const char* K_THRESH  = "thresh";
static const char* K_MOUTHLO = "mouthlo";
static const char* K_MOUTHHI = "mouthhi";
static const char* K_HEADSPD = "headspd";
static const char* K_HEADTMO = "headtmo";
static const char* K_SPKDLY  = "spkdly";
static const char* K_SPKRATE = "spkrate";
static const char* K_PPMAX   = "ppmax";

void configBegin(const BassConfig& d) {
  defaults = d;
  cfg = d;

  Preferences prefs;
  if (!prefs.begin(NAMESPACE, true)) return;

  cfg.soundThreshold = prefs.getInt(K_THRESH,  d.soundThreshold);
  cfg.mouthSpeedMin  = prefs.getInt(K_MOUTHLO, d.mouthSpeedMin);
  cfg.mouthSpeedMax  = prefs.getInt(K_MOUTHHI, d.mouthSpeedMax);
  cfg.headSpeed      = prefs.getInt(K_HEADSPD, d.headSpeed);
  cfg.headTimeoutMs  = prefs.getULong(K_HEADTMO, d.headTimeoutMs);
  cfg.speakDelayMs   = prefs.getULong(K_SPKDLY, d.speakDelayMs);
  cfg.speakRatePct   = prefs.getInt(K_SPKRATE, d.speakRatePct);
  cfg.ppFullScale    = prefs.getInt(K_PPMAX, d.ppFullScale);
  prefs.end();
}

const BassConfig& config() {
  return cfg;
}

bool configSet(const char* key, long value) {
  if (!strcmp(key, K_THRESH)) {
    if (value < 0 || value > 1023) return false;
    cfg.soundThreshold = (int)value;
  } else if (!strcmp(key, K_MOUTHLO)) {
    if (value < 0 || value > 255) return false;
    cfg.mouthSpeedMin = (int)value;
  } else if (!strcmp(key, K_MOUTHHI)) {
    if (value < 0 || value > 255) return false;
    cfg.mouthSpeedMax = (int)value;
  } else if (!strcmp(key, K_HEADSPD)) {
    if (value < 0 || value > 255) return false;
    cfg.headSpeed = (int)value;
  } else if (!strcmp(key, K_HEADTMO)) {
    if (value < 0 || value > 600000) return false;
    cfg.headTimeoutMs = (unsigned long)value;
  } else if (!strcmp(key, K_SPKDLY)) {
    if (value < 0 || value > 5000) return false;
    cfg.speakDelayMs = (unsigned long)value;
  } else if (!strcmp(key, K_SPKRATE)) {
    if (value < 50 || value > 200) return false;
    cfg.speakRatePct = (int)value;
  } else if (!strcmp(key, K_PPMAX)) {
    if (value < 10 || value > 4095) return false;
    cfg.ppFullScale = (int)value;
  } else {
    return false;
  }

  dirty = true;
  return true;
}

void configSave() {
  if (!dirty) return;

  Preferences prefs;
  if (!prefs.begin(NAMESPACE, false)) return;

  prefs.putInt(K_THRESH,  cfg.soundThreshold);
  prefs.putInt(K_MOUTHLO, cfg.mouthSpeedMin);
  prefs.putInt(K_MOUTHHI, cfg.mouthSpeedMax);
  prefs.putInt(K_HEADSPD, cfg.headSpeed);
  prefs.putULong(K_HEADTMO, cfg.headTimeoutMs);
  prefs.putULong(K_SPKDLY, cfg.speakDelayMs);
  prefs.putInt(K_SPKRATE, cfg.speakRatePct);
  prefs.putInt(K_PPMAX, cfg.ppFullScale);
  prefs.end();

  dirty = false;
}

void configReset() {
  cfg = defaults;
  dirty = true;
  configSave();
}

void configFormat(String& dst) {
  dst += K_THRESH;  dst += '=';  dst += cfg.soundThreshold;
  dst += ' ';
  dst += K_MOUTHLO; dst += '=';  dst += cfg.mouthSpeedMin;
  dst += ' ';
  dst += K_MOUTHHI; dst += '=';  dst += cfg.mouthSpeedMax;
  dst += ' ';
  dst += K_HEADSPD; dst += '=';  dst += cfg.headSpeed;
  dst += ' ';
  dst += K_HEADTMO; dst += '=';  dst += cfg.headTimeoutMs;
  dst += ' ';
  dst += K_SPKDLY;  dst += '=';  dst += cfg.speakDelayMs;
  dst += ' ';
  dst += K_SPKRATE; dst += '=';  dst += cfg.speakRatePct;
  dst += ' ';
  dst += K_PPMAX;   dst += '=';  dst += cfg.ppFullScale;
}
