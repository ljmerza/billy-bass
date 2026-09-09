/*
  Voice detection by pitch.

  The 300-3000Hz band-pass in sound.cpp answers "is there midrange energy",
  which a radio playing music satisfies just as well as a person talking. This
  answers a different question: is the sound *periodic* the way a voice is, and
  does its pitch move the way a voice's does.

  Talking and singing are both quasi-periodic - vocal folds running at one
  fundamental somewhere between about 90 and 400Hz - and that fundamental never
  sits still. Speech prosody slides it around continuously; singing steps it
  between notes and adds vibrato on the way. A fan, applause, traffic and
  cymbals have no fundamental at all. A sustained instrument or a synth pad has
  one but holds it far steadier than a larynx can. Polyphonic music has several
  at once, which shows up not as a steady track but as an estimate that jumps
  around at random.

  So three things are measured over about a second of history, and the score is
  the weakest of them:

    voiced - how much of that second had a fundamental at all
    smooth - how much of the pitch track moves in steps a voice could make
    range  - how far the pitch travelled, so a held drone scores zero

  Taking the minimum rather than a weighted sum is deliberate: whichever
  feature says "not a voice" wins, and /status shows which one it was. A sum
  would let two mediocre features outvote one decisive one, and there would be
  no way to see that from outside.

  This is a detector, not a classifier. It will pass a solo cello and it will
  fail a whisper. What it reliably rejects is the case the fish actually loses
  to today: a stereo playing in the same room.

  The estimator is YIN (de Cheveigne & Kawahara 2002) - a squared difference
  function with the cumulative mean normalisation that keeps it off the octave
  below. It runs on a 4kHz tap decimated out of the sampling loop, so a window
  is about a hundred samples and the whole search is a few thousand integer
  operations - well under a millisecond, once per window.

  Known cost: the history features need to fill before the score means
  anything, so the gate opens roughly half a second after someone starts
  talking. The fish misses the first word. Raising the hold time does not fix
  that - it only stops the gate closing between sentences.
*/

#pragma once
#include <Arduino.h>

// decimatedHz is the rate voiceFeed() will be called at, which sets the lag
// range the pitch search covers.
void voiceBegin(int decimatedHz);

// Called at the top of each sampling window, before any voiceFeed().
void voiceFrameReset();

// One decimated sample. Extra samples past the buffer are dropped rather than
// wrapping, so a long window analyses its first ~40ms instead of a splice.
void voiceFeed(int16_t sample);

// Called once the window closes. Runs the pitch search and rolls the history.
void voiceAnalyse();

int  voiceF0();           // Hz for the last window, 0 if nothing periodic
int  voiceConfidence();   // 0-100, periodicity of that window
int  voiceVoicedPct();    // 0-100, history: share of frames with a fundamental
int  voiceSmoothPct();    // 0-100, history: share of frame pairs a voice could make
int  voiceRangePct();     // history: pitch spread as a percent of its own minimum
int  voiceScore();        // 0-100, the weakest of the three sub-scores

// Decimated samples the last window delivered. Under 2x the longest lag (88
// at 4kHz) the window was not analysed and the history was left alone, so a
// low number here with f0=0 means starved, not silent.
int  voiceSamples();

// True while the score has cleared config().voiceScoreMin within the last
// config().voiceHoldMs. This is what the sketch gates the motors on.
bool voiceOpen();
