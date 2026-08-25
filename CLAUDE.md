# Billy Bass

Sound-reactive Big Mouth Billy Bass on an Arduino UNO R4 WiFi with an Adafruit
Motor Shield v2.

## Git

**Never create branches in this repo. Everything goes on `master`.** Commit
directly to `master` — do not branch, do not open PRs for this work.

## Hardware facts that are easy to get wrong

**The motors are forward-only.** Every mechanism in the fish — mouth, head,
tail — drives one way against a return spring into a hard stop. The spring
provides the return stroke. Driving a motor backward stalls it into the stop
and strips the nylon gear train. Nothing in the firmware calls `run(BACKWARD)`
and there is no way to ask for it; `/motor` rejects a `dir=` parameter rather
than ignoring it. Do not add reverse.

Motor ports: **M1 mouth, M2 head, M3 tail.** M4 is free.

The mouth is *pulsed*, not held. Driving it continuously while audio is loud
never gives the return spring a turn, which pins the mouth open for the length
of a phrase. See `articulate` / `mouthon` / `mouthof` in the config.

## Uploading firmware

The board is on WiFi and takes OTA updates. There is usually no USB connection.

```
arduino-cli compile --fqbn arduino:renesas_uno:unor4wifi --output-dir /tmp/bb-build billybass

~/.arduino15/packages/arduino/tools/arduinoOTA/1.3.0/bin/arduinoOTA \
  -address <board-ip> -port 65280 \
  -username arduino -password "$SECRET_OTA_PASS" \
  -sketch /tmp/bb-build/billybass.ino.bin -upload /sketch -b -v
```

`arduino-cli` lives at
`/home/cubxi/Applications/arduino-ide/resources/app/lib/backend/resources/arduino-cli`
(not on `PATH`). `SECRET_OTA_PASS` is in `billybass/arduino_secrets.h`, which is
gitignored.

**The upload tool reports `Error flashing the sketch` on a *successful* flash.**
arduinoOTA 1.3.0 pushes the binary, the board applies it and resets, and the
tool loses the connection before it can confirm — so it exits non-zero either
way. **Verify by checking `uptime` on `/status`**, not by the exit code. If
uptime reset to a small number and the new fields are present, it worked.

`arduino-cli upload --protocol network` does **not** work here — the Renesas
platform has no network upload recipe.

## Tuning over HTTP

Everything is live-tunable without reflashing: `GET /config?key=value`, and
`GET /status` returns telemetry plus the current config. There is a web UI at
the board's address.

When retuning the audio path, `ppmax` must track the actual signal. If `pp`
exceeds `ppmax` the level saturates at full scale, all per-word dynamics are
lost, and the mouth slams identically on every syllable. Check `pp` against
`ppmax` in `/status` before blaming anything else.

`adapt` (adaptive threshold, percent of running mean) fights the pulsed mouth
when set high — the pulse train already handles opening and closing, so a high
adaptive threshold just makes the mouth skip words. `adapt=0` is a reasonable
default now that articulation exists.

## Multi-agent

Other Claude sessions may be editing this repo at the same time. `BassConfig`
in `config.h` is initialised by a **positional** aggregate initializer
(`DEFAULT_CONFIG` in `billybass.ino`) — inserting a struct field in the middle
silently shifts every value after it. Re-check that the field count and order
match after any struct edit.
