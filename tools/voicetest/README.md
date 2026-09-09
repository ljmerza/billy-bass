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

**Scene scoring.** Two seconds each of talking, talking with every other
window starved, singing, a held sung note, a drone, white noise, a clean
two-instrument duet, a full band mix and a mains hum, reported as the three
history features and the resulting score.

The last run before the detector was committed:

```
  talking   voiced  84%  smooth 100%  range  64%   score 100   gate open 100%
  starved   voiced  90%  smooth 100%  range  50%   score 100   gate open 100%
  singing   voiced 100%  smooth  96%  range  34%   score 100   gate open 100%
  held      voiced 100%  smooth 100%  range   4%   score  60   gate open 100%
  drone     voiced 100%  smooth 100%  range   0%   score   0   gate open   0%
  noise     voiced   0%  smooth   0%  range   0%   score   0   gate open   0%
  music     voiced  68%  smooth  57%  range 340%   score  99   gate open 100%
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

`starved` is talking with every other window cut to 42 samples - what a
15ms window delivers, or a 30ms one that a WiFi or HTTP burst took a bite out
of. Those windows are left out of the history rather than recorded as silence,
which is why it scores the same as `talking`. On the board, `vfill` on
`/status` is that sample count; under 88 the window was not analysed, so
`vfill` low with `f0=0` means starved, not quiet.

`held` at 60 is the other number to watch. A sustained sung note has only its
vibrato to prove it came from a larynx, so it clears the default `vscore` of 55
by five points. Raising `vscore` much past 60 trades held notes away.

The signals here are synthetic. They are good enough to catch a broken
estimator and to show which feature rejects which class of sound; they are not
a substitute for pointing the fish at a real stereo and reading `/status`.
