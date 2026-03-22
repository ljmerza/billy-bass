# ADR-0004: FFT Audio Analysis for Voice Isolation

**Status:** Accepted
**Date:** 2026-03-22
**Depends on:** ADR-0001 (ESP32 migration)

## Context

The original Billy Bass used simple analog threshold detection — any sound above a level triggered the motors, including music, bass hits, and background noise. The original code used a clipped ADC range (0-512) as a crude workaround to filter loud non-voice sounds, but this was unreliable.

With the ESP32's processing power and I2S support, real-time FFT analysis is feasible, allowing the fish to react specifically to human voice frequencies.

## Decision

Add real-time FFT processing to isolate human voice frequencies (~300-3000Hz) from music, bass, and background noise. This replaces the simple analog threshold approach.

**Approach:**
- Use the ESP32 I2S peripheral with an I2S MEMS microphone (e.g., INMP441) for high-quality digital audio input, replacing the analog sound sensor
- Use the `arduinoFFT` library to perform frequency-domain analysis on audio samples
- Extract energy in the voice band (300-3000Hz) and use that to drive mouth movement proportionally
- Ignore energy outside the voice band to prevent false triggers from music/bass

**Key parameters to tune:**
- FFT sample size (e.g., 256 or 512 samples)
- Sampling rate (e.g., 8000-16000Hz, sufficient for voice)
- Voice band energy threshold
- Smoothing/averaging window

**Dual input sources:**
- When paired to a smart speaker via Bluetooth (ADR-0002), the FFT analyzes the incoming digital audio stream directly — perfect accuracy
- When no Bluetooth source is connected, the I2S microphone picks up ambient audio as a fallback

## Consequences

- Mouth reacts specifically to voice, not all sound — much more lifelike
- I2S microphone replaces the simple analog sound sensor — different wiring and pin assignments
- FFT processing runs on one ESP32 core, motor control on the other (dual-core advantage)
- Requires tuning to get natural-feeling mouth movement — the WebSocket debug server (ADR-0003) helps with this
- `arduinoFFT` library added as a dependency
