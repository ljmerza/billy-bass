# ADR-0001: Migrate to ESP32 with FFT Audio Analysis and WiFi

**Status:** Accepted
**Date:** 2026-03-22

## Context

The Billy Bass project originally ran on an Arduino Uno with an Adafruit Motor Shield v2, using simple analog threshold detection on a sound sensor to trigger mouth and head DC motors. This approach had several limitations:

- **No frequency discrimination** — any sound above a threshold triggered the motors, including music, bass hits, and background noise. The original code used a clipped ADC range (0-512) as a crude workaround to filter loud non-voice sounds.
- **No connectivity** — debugging required a physical USB serial connection.
- **No remote updates** — firmware changes required physically connecting to the board.
- **Limited resources** — Arduino Uno has 2KB RAM and 16MHz clock, insufficient for real-time audio analysis.

The Arduino has been replaced with an ESP32, which provides WiFi, significantly more RAM (520KB), a faster dual-core processor (240MHz), and native I2S audio input support.

## Decision

### 1. ESP32 as the microcontroller

Replace the Arduino Uno with an ESP32 dev board. The Adafruit Motor Shield v2 communicates over I2C so it remains compatible — the ESP32 I2C pins (default GPIO 21/22) connect to the shield.

**What changes:**
- Board target changes from Arduino Uno to ESP32
- Pin assignments updated for ESP32 GPIO
- Gain access to WiFi, Bluetooth, I2S, and more RAM

### 2. FFT audio analysis for voice isolation

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

### 3. WiFi OTA (Over-the-Air) updates

Use the ESP32 built-in ArduinoOTA library for firmware updates over WiFi. This avoids needing physical access to the board for every code change during development and tuning.

**Approach:**
- Connect to a configured WiFi network on boot
- Start the ArduinoOTA service
- Updates pushed from Arduino IDE or PlatformIO over the local network
- Fallback: USB serial remains available if WiFi is down

### 4. WiFi debug logging

Replace serial-only debug output with network-based logging for remote monitoring.

**Approach:**
- Run a lightweight WebSocket server on the ESP32
- Stream real-time debug data (sound levels, FFT bins, motor states) to any browser on the local network
- Keep serial output as a secondary/fallback debug channel
- Useful for tuning FFT parameters without being physically tethered to the board

## Consequences

**Benefits:**
- Mouth reacts specifically to voice, not all sound — much more lifelike
- Firmware can be updated wirelessly during tuning and development
- Debug data viewable from any device on the network
- Significantly more headroom for future features (Bluetooth audio input, web config UI, etc.)

**Trade-offs:**
- More complex firmware (WiFi, FFT, WebSocket code)
- Requires WiFi network availability (OTA and debug depend on it)
- I2S microphone replaces the simple analog sensor — different wiring
- Power consumption is higher than Arduino Uno (ESP32 WiFi draws ~150mA)
- Circuit pinout diagram will need to be updated for ESP32 GPIO

**Migration path:**
1. Get the current motor control logic running on ESP32 with I2C motor shield
2. Add WiFi connectivity and OTA
3. Swap analog sound sensor for I2S microphone
4. Add FFT analysis and voice-band filtering
5. Add WebSocket debug server
6. Tune FFT parameters for natural mouth movement

## Future Ideas

### Bluetooth A2DP audio input + speaker
The Billy Bass has no speaker. The ESP32 can act as a Bluetooth audio sink (like a BT speaker) using the `BluetoothA2DPSink` library. Pair a phone, stream audio to the fish, and the same audio data feeds both a small internal speaker and the FFT analysis for mouth sync. See ADR-0002 for speaker options and space constraints.

### Web configuration portal
Host a small web UI on the ESP32 for adjusting parameters (FFT thresholds, motor speeds, voice band range, head timeout) without reflashing. Store settings in ESP32 NVS (non-volatile storage). Accessible from any browser on the local network.

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
