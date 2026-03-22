# ADR-0002: Bluetooth Audio Streaming and Speaker Output

**Status:** Proposed
**Date:** 2026-03-22

## Context

The Billy Bass has no built-in speaker capable of playing music or voice audio. The ESP32 supports Bluetooth A2DP (audio streaming), which opens the possibility of receiving audio wirelessly from a phone or computer. However, the physical enclosure has very limited space for additional components.

The fish needs to both play audio and react to it (mouth/head movement via FFT from ADR-0001).

## Options Considered

### Option A: Small internal speaker with I2S DAC amp

Add a compact I2S DAC amplifier board (e.g., MAX98357A) and a small speaker (e.g., 28mm/40mm) inside the Billy Bass enclosure.

- ESP32 receives Bluetooth A2DP audio
- Audio routed to I2S DAC amp → small speaker inside the fish
- Same audio stream fed to FFT for mouth sync
- **Pro:** Self-contained, no external equipment
- **Con:** Limited space, sound quality constrained by tiny speaker, may need to remove some original mechanical parts to fit

### Option B: External Bluetooth speaker + ESP32 as audio passthrough

ESP32 receives audio via Bluetooth from a phone, analyzes it with FFT for motor control, and re-streams or passes audio to a separate external speaker placed nearby (behind the fish, under a shelf, etc.).

- **Pro:** Good sound quality, no enclosure modification needed
- **Con:** Extra device, more complex audio routing, latency between speaker and mouth sync

### Option C: Phone/source plays audio on its own speaker, ESP32 listens with mic

The audio source (phone, computer) plays through its own speaker or a nearby speaker. The ESP32 I2S microphone picks up the audio from the room and drives the motors via FFT.

- **Pro:** Simplest setup, no Bluetooth audio streaming needed on ESP32, no speaker to fit
- **Con:** Ambient noise affects accuracy, mic placement matters, no direct audio stream means less precise sync

## Decision

**Option A (small internal speaker)** is the recommended path for the cleanest result, with Option C as a low-effort fallback.

The MAX98357A + small speaker combo is physically tiny (~15mm x 20mm board + 28mm speaker) and can fit in the Billy Bass cavity with some rearrangement. It keeps everything self-contained and gives the best mouth-to-audio sync since the FFT analyzes the exact same digital stream being played.

If space is truly too tight after prototyping, fall back to Option C — it requires no hardware changes and still gives reasonable results with the I2S mic already planned in ADR-0001.

## Implementation Notes

- ESP32 `BluetoothA2DPSink` library handles receiving Bluetooth audio
- Audio data callback feeds both the I2S DAC output (speaker) and the FFT analysis buffer
- Single audio stream, dual purpose: playback + motor sync
- Speaker wiring: ESP32 I2S pins → MAX98357A → small speaker
- May need to 3D print a small bracket or modify the internal plastic to mount the speaker

## Consequences

- Adds I2S DAC amp board and speaker to the BOM
- Requires physical prototyping to confirm fit inside the enclosure
- Bluetooth audio adds ~100ms latency — acceptable for lip sync but noticeable if compared to a separate speaker playing the same source
- Circuit pinout diagram will need another update for I2S DAC connections
