# ADR-0003: WiFi OTA Updates and Debug Logging

**Status:** Accepted
**Date:** 2026-03-22
**Depends on:** ADR-0001 (ESP32 migration)

## Context

During development and tuning of the Billy Bass, firmware updates require physically connecting a USB cable to the ESP32 inside the fish enclosure. Debugging is limited to serial output over that same USB connection. This is impractical when the fish is mounted on a wall or the enclosure is closed up.

The ESP32 has built-in WiFi, making over-the-air updates and network-based debugging possible.

## Decision

### OTA Updates

Use the ESP32 built-in ArduinoOTA library for firmware updates over WiFi.

**Approach:**
- Connect to a configured WiFi network on boot
- Start the ArduinoOTA service
- Updates pushed from Arduino IDE or PlatformIO over the local network
- Fallback: USB serial remains available if WiFi is down

### WiFi Debug Logging

Replace serial-only debug output with network-based logging for remote monitoring.

**Approach:**
- Run a lightweight WebSocket server on the ESP32
- Stream real-time debug data (sound levels, FFT bins, motor states) to any browser on the local network
- Keep serial output as a secondary/fallback debug channel
- Useful for tuning FFT parameters without being physically tethered to the board

### Web Configuration Portal

Host a small web UI on the ESP32 for adjusting parameters (FFT thresholds, motor speeds, voice band range, head timeout) without reflashing. Store settings in ESP32 NVS (non-volatile storage). Accessible from any browser on the local network.

## Consequences

- Firmware can be updated wirelessly during tuning and development
- Debug data viewable from any device on the network
- Requires WiFi network availability (OTA and debug depend on it)
- Adds WiFi stack to firmware — uses ~40KB additional RAM
- Need to store WiFi credentials (SSID/password) in firmware or NVS
- Power consumption increases when WiFi radio is active (~150mA)
