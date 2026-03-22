# ADR-0001: Migrate from Arduino Uno to ESP32

**Status:** Accepted
**Date:** 2026-03-22

## Context

The Billy Bass project originally ran on an Arduino Uno with an Adafruit Motor Shield v2, using simple analog threshold detection on a sound sensor to trigger mouth and head DC motors. The Arduino Uno has significant limitations:

- **2KB RAM / 16MHz clock** — insufficient for real-time audio analysis
- **No connectivity** — debugging required a physical USB serial connection
- **No remote updates** — firmware changes required physically connecting to the board
- **Limited I/O** — no native I2S, no Bluetooth, no WiFi

The Arduino has been replaced with an ESP32, which unlocks WiFi, Bluetooth, I2S, 520KB RAM, and a 240MHz dual-core processor.

## Decision

Replace the Arduino Uno with an ESP32 dev board. The Adafruit Motor Shield v2 communicates over I2C so it remains compatible — the ESP32 I2C pins (default GPIO 21/22) connect to the shield.

**What changes:**
- Board target changes from Arduino Uno to ESP32
- Pin assignments updated for ESP32 GPIO
- Gain access to WiFi, Bluetooth, I2S, and more RAM
- Power consumption increases (ESP32 WiFi draws ~150mA)
- Circuit pinout diagram will need to be updated for ESP32 GPIO

**Migration path:**
1. Get the current motor control logic running on ESP32 with I2C motor shield
2. Add WiFi (ADR-0003)
3. Add FFT audio analysis (ADR-0004)
4. Add Bluetooth audio + speaker (ADR-0002)

## Consequences

- All existing I2C motor control code remains compatible
- Opens the door for FFT, WiFi, and Bluetooth features in subsequent ADRs
- Higher power consumption than Arduino Uno
- ESP32 dev boards are slightly larger — verify fit in enclosure

## Future Ideas

### Tail motor control
The original Billy Bass has a tail flap mechanism. Add a third motor output on the shield (M3) to move the tail on beat or periodically during music playback. Could use low-frequency FFT bins (bass/beat detection) to drive tail movement.

### LED effects
Add addressable LEDs (WS2812/NeoPixel) inside or around the fish — eyes, mounting plaque, etc. React to sound levels or frequency bands for a visual effect synced with the audio.

### Home Assistant / MQTT integration
Publish motor states, sound levels, and status to an MQTT broker. Trigger the fish remotely from Home Assistant automations — e.g., announce doorbell events, play TTS notifications, or activate on motion sensor triggers.

### Scheduled performances
Store pre-programmed motor sequences (mouth and head choreography) synced to specific audio tracks. Play them on a schedule or trigger via MQTT/web UI — like a singing fish alarm clock.

### Multiple fish sync
If you have more than one Billy Bass, use ESP-NOW (ESP32 peer-to-peer protocol) to synchronize movements across multiple fish for a coordinated performance.
