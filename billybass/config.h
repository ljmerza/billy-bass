/*
  Runtime-tunable parameters, persisted to the RA4M1's data flash via
  Preferences. The HTTP and MQTT adapters are both thin wrappers over this -
  the validation and persistence live here once, not twice.

  Tuning the fish used to cost a full reflash per adjustment. This makes it a
  request.
*/

#pragma once
#include <Arduino.h>

struct BassConfig {
  int soundThreshold;
  int mouthSpeedMin;
  int mouthSpeedMax;
  int headSpeed;
  unsigned long headTimeoutMs;

  // Text-to-motion. speakDelayMs holds the mouth shut at the start of an
  // utterance so the animation can be lined up with audio another device is
  // playing; speakRatePct scales syllable timing to match that audio's pace.
  unsigned long speakDelayMs;
  int speakRatePct;

  // Raw ADC peak-to-peak swing treated as full volume. Lower it for a quiet
  // source, raise it if the fish maxes out on everything.
  int ppFullScale;
};

// Loads persisted values, falling back to defaults for anything unset.
void configBegin(const BassConfig& defaults);

const BassConfig& config();

// Applies one named parameter. Returns false for an unknown key or a value
// outside its valid range, leaving the config untouched. Does not persist -
// call configSave() once after a batch of sets.
bool configSet(const char* key, long value);

// Writes the current values to flash. Skips the write when nothing changed.
void configSave();

// Restores compiled-in defaults and persists them.
void configReset();

// Appends "key=value key=value ..." to dst. Shared by the HTTP and MQTT
// adapters so both report the same thing.
void configFormat(String& dst);
