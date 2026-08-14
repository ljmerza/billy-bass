/*
  Text-driven mouth animation.

  Turns a string into a stream of sound levels in the same units the mapped ADC
  reading uses, so the sketch can substitute speechLevel() for analogRead() and
  every downstream behaviour - threshold, proportional mouth speed, head
  engage/release, telemetry, MQTT - works unchanged. As far as the motor logic
  is concerned the fish is hearing something.

  Syllables are estimated from vowel runs and shaped per vowel, with real pauses
  at commas and sentence ends. This is an approximation of speech, not a
  phonetic model: it looks convincing at conversational speed on a mouth that
  only has open and closed.
*/

#pragma once
#include <Arduino.h>

// Longest utterance accepted. Sized to fit comfortably in RAM alongside the
// network stack; longer text is rejected rather than truncated mid-word.
static const size_t SPEECH_MAX_LEN = 240;

// maxLevel should match the sketch's mapped sensor range, so a wide vowel
// produces the same motor speed a loud sound would.
void speechBegin(int maxLevel);

// Queues text. startDelayMs holds the mouth shut before beginning, to line up
// with audio that another device is about to play. Replaces anything already
// in progress. Returns false if the text is empty or over SPEECH_MAX_LEN.
bool speechSay(const char* text, unsigned long startDelayMs);

void speechStop();

// True while speaking, including the start delay. The sketch should use
// speechLevel() in place of the ADC for as long as this holds.
bool speechActive();

// Synthetic sound level for this instant, 0 while the mouth is closed.
int speechLevel();

// Call every loop().
void speechLoop();
