# ADR-0001: Migrate from Arduino Uno to Arduino UNO R4 WiFi

**Status:** Accepted
**Date:** 2026-03-22

## Context

The Billy Bass project originally ran on an Arduino Uno with an Adafruit Motor Shield v2, using simple analog threshold detection on a sound sensor to trigger mouth and head DC motors. The Arduino Uno has significant limitations:

- **2KB RAM / 16MHz clock** — insufficient for real-time audio analysis
- **No connectivity** — debugging required a physical USB serial connection
- **No remote updates** — firmware changes required physically connecting to the board
- **Limited I/O** — no native I2S, no Bluetooth, no WiFi

The original plan called for a standalone ESP32 dev board, but the **Arduino UNO R4 WiFi [ABX00087]** was chosen instead. The UNO form factor means the Adafruit Motor Shield v2 plugs directly onto the board with zero rewiring — a major practical advantage over a standalone ESP32 which would have required remapping I2C, power, and all pin connections from the shield.

## Hardware Capabilities

### Main MCU — Renesas RA4M1

| Spec | Value |
|------|-------|
| Core | Arm Cortex-M4F |
| Clock | 48MHz |
| Flash | 256KB |
| SRAM | 32KB |
| FPU | Hardware single-precision (useful for FFT) |
| ADC | 14-bit (0–16383) — up from Uno's 10-bit |
| DAC | 12-bit — built-in analog audio output |
| OP AMP | Built-in — can amplify mic input or DAC output |

### Connectivity Co-processor — ESP32-S3

| Spec | Value |
|------|-------|
| WiFi | 802.11 b/g/n |
| Bluetooth | 5.0 LE only — **no Classic Bluetooth** |
| Role | Managed coprocessor, not directly programmable |

**No Classic Bluetooth means no A2DP.** The fish cannot act as a Bluetooth audio sink. This invalidates the original ADR-0002 approach.

### Built-in Peripherals

- **12x8 LED Matrix** — visual effects with zero additional hardware
- **Qwiic / STEMMA QT connector** — easy I2C sensor expansion
- **USB-C** — modern connector for programming and power
- **CAN bus** — available but not needed for this project

## Decision

Replace the Arduino Uno with the Arduino UNO R4 WiFi. The Adafruit Motor Shield v2 stacks directly onto the R4 WiFi's UNO-compatible headers using the same I2C bus (SDA/SCL on the same pins).

### What changes from the original Uno

- Board target changes to `arduino:renesas_uno:unor4wifi`
- 14-bit ADC: `analogRead()` returns 0–16383 instead of 0–1023 — either update `SENSOR_RAW_MAX` or call `analogReadResolution(10)` to preserve existing behavior
- Gain access to WiFi, DAC, OP AMP, LED matrix, Qwiic, and more RAM/flash
- Same physical footprint — Motor Shield v2 plugs right in

### What changes from the ESP32 plan

- **No A2DP** — ESP32-S3 is BLE-only; `BluetoothA2DPSink` library will not work
- **Less RAM** — 32KB SRAM vs ESP32's 520KB; constrains FFT buffer sizes and web server complexity
- **Single-core 48MHz** vs ESP32's dual-core 240MHz — FFT and motor control share one core
- **No I2S on main MCU** — must use analog mic with the 14-bit ADC instead of I2S MEMS mic
- **ESP32-S3 is a coprocessor** — handles WiFi/BLE only, not available for general-purpose code

## Migration Path

### Phase 1 — Board swap and validation

Change the board target to Arduino UNO R4 WiFi. Compile and upload the existing motor control sketch. Verify `analogRead(A0)`, Motor Shield v2 I2C communication, and sound threshold logic all work.

**Code changes required:**
- Add `analogReadResolution(10);` in `setup()` to keep 10-bit behavior during initial migration, OR update `SENSOR_RAW_MAX` from `512` to the proportional 14-bit equivalent
- Verify `#include "utility/Adafruit_MS_PWMServoDriver.h"` resolves — remove if it causes build errors (the main `Adafruit_MotorShield.h` include handles it)

### Phase 2 — WiFi and OTA (ADR-0003, revised)

Add WiFi connectivity using the `WiFiS3` library. Implement OTA updates. Add a lightweight WebSocket debug server. Use RA4M1 EEPROM emulation for storing WiFi credentials and tuning parameters.

### Phase 3 — FFT audio analysis (ADR-0004, revised)

Replace simple analog threshold with FFT-based voice isolation. Use the 14-bit ADC with an analog microphone module (e.g., MAX4466 or MAX9814) instead of an I2S MEMS mic. Use `arduinoFFT` library with 256-sample buffers (safe for 32KB SRAM). The Cortex-M4F hardware FPU keeps FFT computation fast enough for real-time use. The built-in OP AMP could pre-amplify the mic signal before it hits the ADC.

### Phase 4 — Audio output (ADR-0002, revised)

Since A2DP is impossible, the primary audio input for motor sync is **mic pickup** — an analog microphone analyzed by FFT. For audio output from the fish (TTS, sound clips), use the built-in 12-bit DAC amplified by the built-in OP AMP driving a small speaker. Lower fidelity than the I2S DAC amp (MAX98357A) approach but requires zero external audio hardware.

### Phase 5 — LED matrix effects

Use the built-in 12x8 LED matrix for visual feedback: VU meter display, frequency spectrum visualization, status indicators, animations synced to audio. This was a future idea in the original plan — now available with zero additional hardware.

## Impact on Other ADRs

### ADR-0002 (Bluetooth Audio) — Needs revision

The A2DP sink approach is impossible. The `BluetoothA2DPSink` library requires Classic Bluetooth which the ESP32-S3 does not support. Mic pickup becomes the primary audio input. The built-in DAC + OP AMP + small speaker replaces the MAX98357A I2S DAC amp for audio output.

The smart speaker use case changes: instead of the fish replacing the smart speaker's audio output via BT pairing, the fish listens to the smart speaker playing through its own speaker and reacts via mic pickup. This actually avoids the constraint where BT pairing silences the smart speaker.

### ADR-0003 (WiFi/OTA/Debug) — Minor revision

WiFi works via the ESP32-S3 coprocessor using `WiFiS3` instead of ESP32 WiFi libraries. ArduinoOTA should work. WebSocket debug server must be RAM-conscious — keep payloads small, limit to 1–2 concurrent clients. Config storage uses RA4M1 EEPROM emulation instead of ESP32 NVS.

### ADR-0004 (FFT Audio Analysis) — Moderate revision

Analog mic + 14-bit ADC replaces I2S MEMS mic (INMP441). FFT buffer reduced from 512 to 256 samples. Single-core scheduling — use timer interrupts for consistent ADC sampling, process FFT in the main loop. No dual-input-source complexity since there's no BT audio stream; the mic is always the input.

### ADR-0005 (Future Ideas) — Updates

- **LED effects** — promoted from future idea to Phase 5 (built-in LED matrix)
- **Multiple fish sync** — use WiFi UDP multicast or MQTT instead of ESP-NOW (ESP32-specific)
- **Voice command recognition** — not feasible on-device (insufficient RAM); would require cloud service via WiFi
- **Qwiic sensor expansion** — new advantage from built-in Qwiic connector
- **Spotify Connect** — not feasible (no A2DP, insufficient RAM)

## Consequences

- All existing I2C motor control code remains compatible — Motor Shield v2 plugs directly onto the R4 WiFi with zero rewiring
- 14-bit ADC provides meaningfully better audio input resolution than the original Uno
- WiFi, OTA, and web debug capabilities are preserved via the ESP32-S3 coprocessor
- Bluetooth audio streaming (A2DP) is permanently off the table — mic pickup is the audio input strategy
- FFT is feasible but with smaller buffers and single-core scheduling constraints
- Built-in DAC, OP AMP, and LED matrix reduce external component count compared to the ESP32 plan
- 32KB SRAM is the primary constraint limiting feature complexity

## Future Ideas

### Tail motor control
The original Billy Bass has a tail flap mechanism. Add a third motor output on the shield (M3) to move the tail on beat or periodically during music playback. Could use low-frequency FFT bins (bass/beat detection) to drive tail movement.

### Home Assistant / MQTT integration
Publish motor states, sound levels, and status to an MQTT broker. Trigger the fish remotely from Home Assistant automations — e.g., announce doorbell events, play TTS notifications, or activate on motion sensor triggers.

### Scheduled performances
Store pre-programmed motor sequences (mouth and head choreography) synced to specific audio tracks. Play them on a schedule or trigger via MQTT/web UI.

### Multiple fish sync
Use WiFi UDP multicast or MQTT to synchronize movements across multiple fish for a coordinated performance. (ESP-NOW is not available — it is ESP32-specific and the ESP32-S3 on the R4 WiFi is not directly programmable.)

### BLE presence detection
Use BLE advertising for presence detection — phone nearby triggers the fish. Or expose BLE characteristics for a simple mobile app control interface.

### WiFi audio streaming
Investigate receiving audio over WiFi (HTTP stream, UDP audio packets from a local server) as a potential replacement for BT audio. RAM-constrained but worth exploring.

### Qwiic sensor expansion
The built-in Qwiic connector makes adding I2C sensors trivial — gesture sensor for hands-free control, environmental sensors for context-aware behavior, etc.
