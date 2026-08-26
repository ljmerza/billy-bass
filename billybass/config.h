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

  // Head. It swings out on a spring-returned linkage, so travel is set by how
  // long the motor drives rather than by any position feedback: headSpeed for
  // headMoveMs is the stroke, and a longer stroke means further out until it
  // reaches the mechanical stop. headHoldSpeed then keeps it there without
  // stalling the gear train at full power - 0 lets the spring pull it straight
  // back once the stroke ends. headTimeoutMs is how long it stays out, measured
  // from the last sound, before the motor releases entirely.
  int headSpeed;
  unsigned long headMoveMs;
  int headHoldSpeed;
  unsigned long headTimeoutMs;

  // Tail. It flaps while the fish is talking: driven forward for tailFlapMs,
  // then released for tailFlapMs so the return spring pulls it back. Forward
  // and release only - the tail has the same spring-and-stop mechanism as the
  // mouth and head, so there is nothing to reverse into. tailEnabled 0 parks it
  // and leaves the mouth and head working, for a quieter fish or to take a
  // suspect tail motor out of the picture while tuning.
  int tailEnabled;
  int tailSpeed;
  unsigned long tailFlapMs;

  // Text-to-motion. speakDelayMs holds the mouth shut at the start of an
  // utterance so the animation can be lined up with audio another device is
  // playing; speakRatePct scales syllable timing to match that audio's pace.
  unsigned long speakDelayMs;
  int speakRatePct;

  // Raw ADC peak-to-peak swing treated as full volume. Lower it for a quiet
  // source, raise it if the fish maxes out on everything.
  int ppFullScale;

  // 1 to measure only the 300-3000Hz voice band, 0 to measure everything. Off
  // is for comparison during tuning - with music playing, a full-band reading
  // stays high through the whole track and holds the mouth open.
  int voiceFilter;

  // Mouth articulation. With this on the mouth is pulsed rather than held: each
  // rise past the threshold fires one bounded drive of mouthOnMs, followed by a
  // guaranteed mouthOffMs closed. Holding the motor on for a whole phrase never
  // lets the return spring close the mouth, which is what made it sit open.
  // 0 restores the plain proportional hold, for comparison.
  int articulate;
  unsigned long mouthOnMs;
  unsigned long mouthOffMs;

  // Adaptive threshold, as a percentage of the running mean level. The mouth
  // opens when the level clears that percentage, so it tracks peaks against
  // whatever the current volume is instead of a fixed number that only suits
  // one setting. 0 disables it and leaves soundThreshold as the only gate;
  // soundThreshold is always the floor, so this can only raise the bar.
  int adaptPct;

  // Quiet head drive. The shield chops its PWM at about 1.5kHz - the middle of
  // the voice band - and the head is the one motor held on for seconds at a
  // time, so a part-speed hold puts a steady buzz on the ground the audio amp
  // shares. Full on and released are the only two settings that do not switch.
  // headQuiet 1 uses just those, and gets a partial hold by alternating them:
  // energised for headHoldOnMs, then released for headHoldOffMs, repeating, so
  // torque averages over that cycle instead of over each PWM period. headSpeed
  // and headHoldSpeed do not apply in this mode - the stroke runs full on, so
  // headMoveMs alone sets how far the head swings out, and the on/off ratio
  // sets how firmly it is held there. 0 restores the proportional PWM drive,
  // for comparison.
  int headQuiet;
  unsigned long headHoldOnMs;
  unsigned long headHoldOffMs;

  // What makes the tail flap. 0 is speech only - it moves for a /speak
  // utterance and stays still for everything else, so it never reacts to the
  // microphone. 1 also flaps it whenever the head is out, which is the fish's
  // "reacting to sound" window: the head latches on the first sound over the
  // threshold and holds until headTimeoutMs after the last one. Riding that
  // latch rather than the raw threshold is deliberate - the level crosses back
  // and forth several times a second on real audio, and gating the tail on it
  // directly would abandon every drive stroke halfway and leave the tail
  // twitching instead of flapping.
  int tailSoundDriven;

  // How long the button has to be held to count as a long press rather than a
  // short one. The gesture is decided when you let go, so this is compared
  // against the finished press - there is no partial firing on the way down.
  // Appended at the end: DEFAULT_CONFIG in billybass.ino is a positional
  // initializer, so a field inserted anywhere else silently shifts every value
  // after it.
  unsigned long btnLongMs;
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
