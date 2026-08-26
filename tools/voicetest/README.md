# voicetest

Host-side harness for `billybass/voice.cpp`, the pitch-based voice detector.

```
make && ./voicetest
```

It compiles the sketch's real `voice.cpp` against a stub `Arduino.h` and a stub
`config()`, then runs it on generated signals. Two things get checked:

**Pitch accuracy.** The tap that feeds the detector on the board has already
been high-passed at 300Hz, so a real voice arrives with its fundamental mostly
gone and only its middle harmonics left. The test generates that same shape, so
a change that only works when the fundamental is present fails here instead of
on the fish.

**Scene scoring.** Two seconds each of talking, singing, a held sung note, a
drone, white noise, a clean two-instrument duet, a full band mix and a mains
hum, reported as the three history features and the resulting score.

The last run before the detector was committed:

```
  talking   voiced  90%  smooth  96%  range  59%   score 100   gate open 100%
  singing   voiced 100%  smooth  96%  range  42%   score 100   gate open 100%
  held      voiced 100%  smooth 100%  range   4%   score  60   gate open 100%
  drone     voiced 100%  smooth 100%  range   0%   score   0   gate open   0%
  noise     voiced   0%  smooth   0%  range   0%   score   0   gate open   0%
  music     voiced  59%  smooth  81%  range 102%   score 100   gate open 100%
  band      voiced   3%  smooth   0%  range   0%   score   0   gate open   0%
  hum       voiced 100%  smooth 100%  range   0%   score   0   gate open   0%
```

Read `music` and `band` together, because they are the same two melodic lines
and they are the whole story. Add a drum kit and a noise floor and the estimate
stops tracking anything, which is the case the fish actually meets - a track
playing in the room. Take those away and two clean melodic lines are
indistinguishable from two people singing, because by every measurement here
they *are*. Pitch alone cannot separate them, and no amount of tuning these
three numbers will change that. A band-energy ratio would.

`held` at 60 is the other number to watch. A sustained sung note has only its
vibrato to prove it came from a larynx, so it clears the default `vscore` of 55
by five points. Raising `vscore` much past 60 trades held notes away.

The signals here are synthetic. They are good enough to catch a broken
estimator and to show which feature rejects which class of sound; they are not
a substitute for pointing the fish at a real stereo and reading `/status`.
