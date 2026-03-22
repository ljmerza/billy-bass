# ADR-0005: Future Improvements and Ideas

**Status:** Draft
**Date:** 2026-03-22

A collection of potential improvements and feature ideas not covered by existing ADRs. These are not committed decisions — they are documented here for future consideration.

## Hardware Improvements

### Servo replacement for DC motors
Replace the original DC motors with micro servos for more precise, position-controlled mouth and head movement. DC motors only support on/off/speed control, while servos allow specific angles — enabling more realistic lip sync patterns (partially open, fully open, closed).

### Capacitive touch sensor
Add a capacitive touch sensor to the fish body. Trigger different reactions when someone touches or pets the fish — play a specific sound clip, wiggle the tail, or announce something.

### PIR motion sensor
Add a passive infrared motion sensor so the fish activates when someone walks by. Could greet people, tell jokes, or deliver announcements triggered by presence.

### Battery power
Add a rechargeable LiPo battery with a charging circuit (e.g., TP4056) so the fish can operate without a wall plug. ESP32 deep sleep between activations to extend battery life. Wake on BT connection, motion, or touch.

### Improved enclosure
3D print internal brackets to cleanly mount the ESP32, speaker, and any additional sensors. Design a replacement back panel with ventilation holes for the speaker and access ports for USB/charging.

## Software Improvements

### Text-to-speech (TTS)
Use an online TTS API (Google Cloud TTS, Amazon Polly) or an on-device TTS library to generate speech. The fish could speak custom messages, read notifications, or respond to voice commands with synthesized speech played through the internal speaker.

### Voice command recognition
Use the ESP32's microphone input with a wake-word detection library (e.g., ESP-SR or Picovoice Porcupine) to respond to voice commands. Simple commands like "tell me a joke," "what time is it," or "sing a song" without needing a cloud assistant.

### Sound effect library
Store a collection of short audio clips (splash sounds, drum rimshots, laugh tracks, catchphrases) in SPIFFS/LittleFS on the ESP32. Trigger them via MQTT, web UI, button press, or on a schedule.

### Mouth movement patterns
Pre-define different mouth movement styles beyond simple proportional response:
- **Talk mode** — rapid open/close for speech
- **Sing mode** — smoother, wider movements synced to music amplitude
- **Laugh mode** — quick bursts of movement
- **Sleep mode** — occasional slow mouth movement (like snoring)

Select the pattern based on FFT analysis or manual override via web UI.

### Music beat detection
Beyond voice isolation (ADR-0004), add beat/tempo detection using low-frequency FFT bins. Sync tail and head movements to the beat of music while mouth follows vocals. Different body parts react to different frequency bands.

### Spotify / media integration
Use Spotify Connect or similar protocol so the fish appears as a playback device. Select it as a speaker in Spotify and the fish plays music while lip-syncing. Could also display now-playing info on the web debug dashboard.

### Recording and playback
Record a sequence of motor movements and audio, then replay it. Useful for creating choreographed performances that can be triggered later without the original audio source.

### Holiday / seasonal modes
Pre-programmed behaviors for holidays — Christmas carols, Halloween sounds, birthday songs. Auto-activate based on date or trigger via Home Assistant.

## Network / Integration

### Discord / Slack bot
Connect the fish to a Discord or Slack bot. Send a message to a channel and the fish speaks it out loud using TTS. Could also post the fish's "status" (sound levels, uptime) to a channel.

### Notification relay
Forward phone notifications (via Tasker/IFTTT/Home Assistant) to the fish. Doorbell rings → fish announces "someone's at the door." Timer goes off → fish tells you. Weather alert → fish warns you.

### REST API
Expose a simple REST API on the ESP32 for direct control:
- `POST /speak` — TTS a message
- `POST /play` — play a stored sound clip
- `POST /move` — trigger a specific motor pattern
- `GET /status` — return sensor readings and motor states

### Camera integration
Pair with a nearby camera (ESP32-CAM module or IP camera). Fish reacts when it "sees" someone — could use simple motion detection or face detection to trigger greetings.
