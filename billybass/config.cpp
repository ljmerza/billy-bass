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
static const char* K_HEADMV  = "headmv";
static const char* K_HEADHLD = "headhld";
static const char* K_HEADTMO = "headtmo";
static const char* K_TAILEN  = "tailen";
static const char* K_TAILSPD = "tailspd";
static const char* K_TAILMS  = "tailms";
static const char* K_SPKDLY  = "spkdly";
static const char* K_SPKRATE = "spkrate";
static const char* K_PPMAX   = "ppmax";
static const char* K_VOICE   = "voice";
static const char* K_ADAPT   = "adapt";
static const char* K_ARTIC   = "artic";
static const char* K_MOUTHON = "mouthon";
static const char* K_MOUTHOF = "mouthof";
static const char* K_HEADQ   = "headq";
static const char* K_HEADHON = "headhon";
static const char* K_HEADHOF = "headhof";
static const char* K_TAILSND = "tailsnd";
static const char* K_BTNLONG = "btnlong";

void configBegin(const BassConfig& d) {
  defaults = d;
  cfg = d;

  Preferences prefs;
  if (!prefs.begin(NAMESPACE, true)) return;

  cfg.soundThreshold = prefs.getInt(K_THRESH,  d.soundThreshold);
  cfg.mouthSpeedMin  = prefs.getInt(K_MOUTHLO, d.mouthSpeedMin);
  cfg.mouthSpeedMax  = prefs.getInt(K_MOUTHHI, d.mouthSpeedMax);
  cfg.headSpeed      = prefs.getInt(K_HEADSPD, d.headSpeed);
  cfg.headMoveMs     = prefs.getULong(K_HEADMV, d.headMoveMs);
  cfg.headHoldSpeed  = prefs.getInt(K_HEADHLD, d.headHoldSpeed);
  cfg.headTimeoutMs  = prefs.getULong(K_HEADTMO, d.headTimeoutMs);
  cfg.tailEnabled    = prefs.getInt(K_TAILEN, d.tailEnabled);
  cfg.tailSpeed      = prefs.getInt(K_TAILSPD, d.tailSpeed);
  cfg.tailFlapMs     = prefs.getULong(K_TAILMS, d.tailFlapMs);
  cfg.speakDelayMs   = prefs.getULong(K_SPKDLY, d.speakDelayMs);
  cfg.speakRatePct   = prefs.getInt(K_SPKRATE, d.speakRatePct);
  cfg.ppFullScale    = prefs.getInt(K_PPMAX, d.ppFullScale);
  cfg.voiceFilter    = prefs.getInt(K_VOICE, d.voiceFilter);
  cfg.adaptPct       = prefs.getInt(K_ADAPT, d.adaptPct);
  cfg.articulate     = prefs.getInt(K_ARTIC, d.articulate);
  cfg.mouthOnMs      = prefs.getULong(K_MOUTHON, d.mouthOnMs);
  cfg.mouthOffMs     = prefs.getULong(K_MOUTHOF, d.mouthOffMs);
  cfg.headQuiet      = prefs.getInt(K_HEADQ, d.headQuiet);
  cfg.headHoldOnMs   = prefs.getULong(K_HEADHON, d.headHoldOnMs);
  cfg.headHoldOffMs  = prefs.getULong(K_HEADHOF, d.headHoldOffMs);
  cfg.tailSoundDriven = prefs.getInt(K_TAILSND, d.tailSoundDriven);
  cfg.btnLongMs      = prefs.getULong(K_BTNLONG, d.btnLongMs);
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
  } else if (!strcmp(key, K_HEADMV)) {
    // Capped at the motor test's ceiling: past a few seconds the head is
    // already against its stop and the extra drive is pure stall.
    if (value < 0 || value > 5000) return false;
    cfg.headMoveMs = (unsigned long)value;
  } else if (!strcmp(key, K_HEADHLD)) {
    if (value < 0 || value > 255) return false;
    cfg.headHoldSpeed = (int)value;
  } else if (!strcmp(key, K_HEADTMO)) {
    if (value < 0 || value > 600000) return false;
    cfg.headTimeoutMs = (unsigned long)value;
  } else if (!strcmp(key, K_TAILEN)) {
    if (value < 0 || value > 1) return false;
    cfg.tailEnabled = (int)value;
  } else if (!strcmp(key, K_TAILSPD)) {
    if (value < 0 || value > 255) return false;
    cfg.tailSpeed = (int)value;
  } else if (!strcmp(key, K_TAILMS)) {
    // Floor of 40ms: below that the spring cannot complete a return stroke and
    // the tail just buzzes against the linkage.
    if (value < 40 || value > 2000) return false;
    cfg.tailFlapMs = (unsigned long)value;
  } else if (!strcmp(key, K_SPKDLY)) {
    if (value < 0 || value > 5000) return false;
    cfg.speakDelayMs = (unsigned long)value;
  } else if (!strcmp(key, K_SPKRATE)) {
    if (value < 50 || value > 200) return false;
    cfg.speakRatePct = (int)value;
  } else if (!strcmp(key, K_VOICE)) {
    if (value < 0 || value > 1) return false;
    cfg.voiceFilter = (int)value;
  } else if (!strcmp(key, K_ADAPT)) {
    // Ceiling of 300 rather than 100: values above 100 mean "only peaks above
    // the running mean", which is what actually makes the mouth flap.
    if (value < 0 || value > 300) return false;
    cfg.adaptPct = (int)value;
  } else if (!strcmp(key, K_ARTIC)) {
    if (value < 0 || value > 1) return false;
    cfg.articulate = (int)value;
  } else if (!strcmp(key, K_MOUTHON)) {
    // Floor of 20ms: shorter than that and the motor never overcomes its own
    // inertia, so the mouth twitches instead of opening.
    if (value < 20 || value > 1000) return false;
    cfg.mouthOnMs = (unsigned long)value;
  } else if (!strcmp(key, K_MOUTHOF)) {
    if (value < 20 || value > 1000) return false;
    cfg.mouthOffMs = (unsigned long)value;
  } else if (!strcmp(key, K_HEADQ)) {
    if (value < 0 || value > 1) return false;
    cfg.headQuiet = (int)value;
  } else if (!strcmp(key, K_HEADHON)) {
    // 0 means no hold at all, the same as headHoldSpeed 0: the stroke ends and
    // the spring takes the head straight back. Anything positive has the same
    // 20ms floor as the mouth pulse - below that the gear train never moves.
    if (value != 0 && (value < 20 || value > 1000)) return false;
    cfg.headHoldOnMs = (unsigned long)value;
  } else if (!strcmp(key, K_HEADHOF)) {
    // Floor of 20ms rather than 0: a zero release turns the cycle back into a
    // continuous stall at full power, which is what the hold exists to avoid.
    if (value < 20 || value > 2000) return false;
    cfg.headHoldOffMs = (unsigned long)value;
  } else if (!strcmp(key, K_TAILSND)) {
    if (value < 0 || value > 1) return false;
    cfg.tailSoundDriven = (int)value;
  } else if (!strcmp(key, K_BTNLONG)) {
    // Floor of 200ms: below that an ordinary press starts registering as a
    // hold. Ceiling of 5000 so a long press cannot become unreachable.
    if (value < 200 || value > 5000) return false;
    cfg.btnLongMs = (unsigned long)value;
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
  prefs.putULong(K_HEADMV, cfg.headMoveMs);
  prefs.putInt(K_HEADHLD, cfg.headHoldSpeed);
  prefs.putULong(K_HEADTMO, cfg.headTimeoutMs);
  prefs.putInt(K_TAILEN, cfg.tailEnabled);
  prefs.putInt(K_TAILSPD, cfg.tailSpeed);
  prefs.putULong(K_TAILMS, cfg.tailFlapMs);
  prefs.putULong(K_SPKDLY, cfg.speakDelayMs);
  prefs.putInt(K_SPKRATE, cfg.speakRatePct);
  prefs.putInt(K_PPMAX, cfg.ppFullScale);
  prefs.putInt(K_VOICE, cfg.voiceFilter);
  prefs.putInt(K_ADAPT, cfg.adaptPct);
  prefs.putInt(K_ARTIC, cfg.articulate);
  prefs.putULong(K_MOUTHON, cfg.mouthOnMs);
  prefs.putULong(K_MOUTHOF, cfg.mouthOffMs);
  prefs.putInt(K_HEADQ, cfg.headQuiet);
  prefs.putULong(K_HEADHON, cfg.headHoldOnMs);
  prefs.putULong(K_HEADHOF, cfg.headHoldOffMs);
  prefs.putInt(K_TAILSND, cfg.tailSoundDriven);
  prefs.putULong(K_BTNLONG, cfg.btnLongMs);
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
  dst += K_HEADMV;  dst += '=';  dst += cfg.headMoveMs;
  dst += ' ';
  dst += K_HEADHLD; dst += '=';  dst += cfg.headHoldSpeed;
  dst += ' ';
  dst += K_HEADTMO; dst += '=';  dst += cfg.headTimeoutMs;
  dst += ' ';
  dst += K_TAILEN;  dst += '=';  dst += cfg.tailEnabled;
  dst += ' ';
  dst += K_TAILSPD; dst += '=';  dst += cfg.tailSpeed;
  dst += ' ';
  dst += K_TAILMS;  dst += '=';  dst += cfg.tailFlapMs;
  dst += ' ';
  dst += K_SPKDLY;  dst += '=';  dst += cfg.speakDelayMs;
  dst += ' ';
  dst += K_SPKRATE; dst += '=';  dst += cfg.speakRatePct;
  dst += ' ';
  dst += K_PPMAX;   dst += '=';  dst += cfg.ppFullScale;
  dst += ' ';
  dst += K_VOICE;   dst += '=';  dst += cfg.voiceFilter;
  dst += ' ';
  dst += K_ADAPT;   dst += '=';  dst += cfg.adaptPct;
  dst += ' ';
  dst += K_ARTIC;   dst += '=';  dst += cfg.articulate;
  dst += ' ';
  dst += K_MOUTHON; dst += '=';  dst += cfg.mouthOnMs;
  dst += ' ';
  dst += K_MOUTHOF; dst += '=';  dst += cfg.mouthOffMs;
  dst += ' ';
  dst += K_HEADQ;   dst += '=';  dst += cfg.headQuiet;
  dst += ' ';
  dst += K_HEADHON; dst += '=';  dst += cfg.headHoldOnMs;
  dst += ' ';
  dst += K_HEADHOF; dst += '=';  dst += cfg.headHoldOffMs;
  dst += ' ';
  dst += K_TAILSND; dst += '=';  dst += cfg.tailSoundDriven;
  dst += ' ';
  dst += K_BTNLONG; dst += '=';  dst += cfg.btnLongMs;
}
