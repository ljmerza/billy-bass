# ADR-0002: Bluetooth Audio Streaming and Speaker Output

**Status:** Accepted
**Date:** 2026-03-22

## Context

The Billy Bass has no built-in speaker capable of playing music or voice audio. The ESP32 supports Bluetooth A2DP (audio streaming), which opens the possibility of receiving audio wirelessly from a phone or computer. However, the physical enclosure has very limited space for additional components.

The fish needs to both play audio and react to it (mouth/head movement via FFT from ADR-0001).

A key use case is pairing with a Google Home or Alexa smart speaker so the Billy Bass acts as the voice assistant's output — the fish literally becomes the smart speaker.

## Smart Speaker Constraint

When a Google Home or Alexa pairs to an external Bluetooth speaker, it routes audio **to** that speaker and stops playing through its own. You cannot get both simultaneously. This means:

- The fish must have its own speaker to play the smart speaker's audio
- There is no way to use the smart speaker's built-in speaker AND send the same audio stream to the ESP32 over Bluetooth at the same time
- The ESP32 must act as the BT audio sink (receiver), play the audio itself, and feed the same digital stream to FFT for motor sync

## Options Considered

### Option A: Fish becomes the smart speaker (Recommended)

The ESP32 pairs as a Bluetooth A2DP sink to a Google Home, Alexa, phone, or any audio source. Audio streams to the ESP32, which plays it through a small internal speaker (I2S DAC amp) and simultaneously feeds the digital audio to FFT for mouth/head sync.

- Smart speaker handles voice assistant duties (wake word, commands, responses)
- All audio output comes from the fish — it IS the speaker
- Same digital stream drives both playback and motor sync (perfect sync)
- **Pro:** Self-contained, best possible mouth sync, the funniest option
- **Con:** Limited space for speaker, sound quality constrained by small enclosure

**Hardware:** MAX98357A I2S DAC amp (~15mm x 20mm) + small speaker (28mm/40mm)

### Option B: External Bluetooth speaker + ESP32 as audio passthrough

ESP32 receives audio via Bluetooth from a phone, analyzes it with FFT for motor control, and re-streams or passes audio to a separate external speaker placed nearby (behind the fish, under a shelf, etc.).

- **Pro:** Good sound quality, no enclosure modification needed
- **Con:** Extra device, more complex audio routing, latency between speaker and mouth sync

### Option C: Mic pickup (fallback)

The audio source (phone, smart speaker, computer) plays through its own speaker. The ESP32 I2S microphone picks up the audio from the room and drives the motors via FFT.

- **Pro:** Simplest setup, no Bluetooth audio streaming needed on ESP32, no speaker to fit
- **Con:** Ambient noise affects accuracy, mic placement matters, no direct audio stream means less precise sync, cannot use with smart speaker BT pairing since that silences the smart speaker

### Option D: Aux cable from smart speaker

Some older smart speakers have a 3.5mm aux output. Wire it directly to ESP32 ADC input for a clean analog signal.

- **Pro:** Reliable signal, no BT complexity
- **Con:** Wired, most newer smart speakers dropped the aux port, still need a separate speaker for output

## Decision

**Option A (fish becomes the smart speaker)** is the best approach.

The Billy Bass becomes a Bluetooth speaker that a Google Home, Alexa, or phone pairs to. Audio plays from inside the fish while the same digital stream drives FFT mouth sync. This gives perfect audio-to-motor synchronization and is the most entertaining result — your voice assistant talks through a singing fish.

The MAX98357A + small speaker combo is physically tiny and can fit in the Billy Bass cavity with some rearrangement. If space is truly too tight after prototyping, Option C (mic pickup) is the zero-hardware fallback, but it won't work when paired to a smart speaker since that silences the smart speaker's own output.

## Implementation Notes

- ESP32 `BluetoothA2DPSink` library handles receiving Bluetooth audio
- Audio data callback feeds both the I2S DAC output (speaker) and the FFT analysis buffer
- Single audio stream, dual purpose: playback + motor sync
- Speaker wiring: ESP32 I2S pins → MAX98357A → small speaker
- May need to 3D print a small bracket or modify the internal plastic to mount the speaker
- ESP32 appears as a named Bluetooth device (e.g., "Billy Bass") that the smart speaker or phone can pair to

## Consequences

- Adds I2S DAC amp board and speaker to the BOM
- Requires physical prototyping to confirm fit inside the enclosure
- Bluetooth audio adds ~100ms latency — acceptable for lip sync but noticeable if compared side-by-side with the source
- Circuit pinout diagram will need another update for I2S DAC connections
- When paired to a smart speaker, the smart speaker's own speaker goes silent — all audio comes from the fish
