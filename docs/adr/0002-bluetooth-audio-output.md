# ADR-0002: Bluetooth Audio and ADC Input Circuit

**Status:** Accepted (revised)
**Date:** 2026-03-22
**Depends on:** ADR-0001

## Context

The Billy Bass needs to receive audio wirelessly (from a phone, Google Home, or Alexa) and react to it with mouth/head movements driven by FFT analysis (ADR-0004).

ADR-0001 migrated the project from ESP32 to the Arduino UNO R4 WiFi. The R4 WiFi's ESP32-S3 coprocessor supports **BLE only — no Classic Bluetooth, no A2DP**. This means the Arduino cannot act as a Bluetooth audio sink. The original plan (ESP32 A2DP + MAX98357A I2S DAC amp + new speaker) is no longer possible.

However, the Billy Bass **already has a built-in amplifier, speaker, and 3.5mm headphone jack**. No new audio output hardware is needed — the fish can play audio through its existing circuit.

## Options Considered

### Option A: ESP32 A2DP sink + MAX98357A + new speaker (original plan)

The ESP32 pairs as a Bluetooth A2DP sink, receives audio, plays it through a MAX98357A I2S DAC amp and small internal speaker, and feeds the same digital stream to FFT.

- **Ruled out** — R4 WiFi has no A2DP (BLE only) and no I2S on the main MCU

### Option B: External BT adapter + Y-splitter + existing speaker + ADC input (Recommended)

A standalone Bluetooth 5.0 audio receiver (cheap A2DP dongle with 3.5mm output) receives audio. A 3.5mm Y-splitter sends the signal to both the existing Billy Bass amp/speaker and a passive conditioning circuit feeding the Arduino's 14-bit ADC for FFT analysis.

- **Pro:** Reuses existing speaker/amp, simplest wiring, ~$1 in passive components, clean analog signal for FFT
- **Pro:** BT adapter handles A2DP that the R4 WiFi cannot do natively
- **Pro:** No enclosure modification needed — the 3.5mm jack is already there
- **Con:** Extra external device (BT adapter), requires USB power for the adapter

### Option C: Mic pickup only (fallback)

The audio source plays through its own speaker. An analog microphone on the Arduino picks up room audio and drives motors via FFT.

- **Pro:** Simplest setup, no BT adapter needed
- **Con:** Ambient noise, less precise sync, cannot pair with smart speaker (pairing silences the smart speaker)

### Option D: Aux cable from audio source

Wire a 3.5mm cable directly from a phone/computer/speaker to the Y-splitter (same circuit as Option B, just wired instead of wireless).

- **Pro:** Reliable signal, no BT adapter, zero latency
- **Con:** Wired, limits placement flexibility

## Decision

**Option B: External BT adapter + Y-splitter + passive ADC conditioning circuit.**

A standalone Bluetooth 5.0 receiver pairs with the audio source (phone, Google Home, Alexa). Its 3.5mm output goes through a Y-splitter:

- **One leg → existing Billy Bass amp/speaker** (audio playback)
- **Other leg → passive conditioning circuit → Arduino A0** (FFT analysis for motor sync)

This cleanly separates concerns: the BT adapter handles wireless audio, the existing hardware handles playback, and the Arduino handles motor control. Option D (aux cable) uses the exact same conditioning circuit and is a viable wired alternative.

## Smart Speaker Use Case

When a Google Home or Alexa pairs to the BT adapter, it routes all audio output to the fish. The smart speaker's own speaker goes silent. The fish becomes the smart speaker's voice — the same entertaining result as the original plan, just using an external BT adapter instead of on-chip A2DP.

## ADC Conditioning Circuit

The headphone-level audio signal is AC, centered at 0V, swinging ±0.5V to ±1V peak. The Arduino's ADC reads 0–5V only. Negative voltages would clip and could damage the pin. This passive circuit biases the signal to mid-range and protects the ADC.

### Schematic

```
BT Adapter 3.5mm TIP (via Y-splitter)
        │
       [C1] 1µF film capacitor (AC coupling — blocks DC, passes audio)
        │
        ├───[R1 100kΩ]──── Arduino 5V    ┐
        │                                  ├─ Bias divider → 2.5V DC offset
        ├───[R2 100kΩ]──── Arduino GND   ┘
        │
        ├───[D1 BAT43]────► Arduino 5V    (positive clamp — Schottky)
        ├───[D2 BAT43]◄──── Arduino GND   (negative clamp — Schottky)
        │
        └───[R3 3.3kΩ]──┬── Arduino A0 (14-bit ADC)
                         │
                        [C2] 10nF ceramic (anti-alias LPF)
                         │
                        GND

3.5mm SLEEVE ──── Arduino GND (shared ground reference)
```

### Signal path explanation

1. **C1 (1µF film)** — AC coupling capacitor. Blocks any DC offset from the BT adapter, passes only the audio AC signal. With the 50kΩ bias impedance (R1‖R2), the low-frequency cutoff is 3.2Hz — far below the 300Hz voice band floor.

2. **R1 + R2 (100kΩ each)** — Voltage divider creating a 2.5V DC bias point. The AC audio signal rides on top of this bias, centering the waveform in the middle of the 0–5V ADC range. The 50kΩ parallel impedance is negligible load on the BT adapter (designed for 16–32Ω headphones).

3. **D1 + D2 (BAT43 Schottky diodes)** — Voltage clamps. If the signal somehow exceeds 5.3V, D1 conducts to the 5V rail. If it drops below −0.3V, D2 conducts to GND. Schottky diodes are used for their low 0.3V forward drop. During normal operation (signal swings ±1V around 2.5V = 1.5V to 3.5V), the diodes never conduct. They are protection only.

4. **R3 (3.3kΩ)** — Series current limiter. Protects the ADC pin and limits current through clamp diodes during a fault to max 5V/3.3kΩ ≈ 1.5mA. Also forms the resistive half of the anti-alias filter.

5. **C2 (10nF ceramic)** — Anti-alias low-pass filter. With R3 = 3.3kΩ, the cutoff frequency is 1/(2π × 3300 × 10×10⁻⁹) ≈ **4.8kHz**. This matches the Nyquist limit for an 8kHz FFT sample rate and covers the 300–3000Hz voice band targeted by ADR-0004.

### Component summary

| Ref | Value | Purpose |
|-----|-------|---------|
| C1 | 1µF film | AC coupling (DC blocking) |
| R1 | 100kΩ | Bias divider upper half (→ 5V) |
| R2 | 100kΩ | Bias divider lower half (→ GND) |
| D1 | BAT43 or 1N5819 | Positive clamp to 5V rail |
| D2 | BAT43 or 1N5819 | Negative clamp to GND rail |
| R3 | 3.3kΩ | Series protection + anti-alias filter |
| C2 | 10nF ceramic | Anti-alias LPF (fc ≈ 4.8kHz) |

### Design choices

**5V ADC reference (not 3.3V):** With 2.5V bias, headphone signals (±0.5–1V) swing between 1.5V and 3.5V — well within the 0–5V range with ±1.5V of headroom. No attenuation stage needed. The 14-bit ADC gives 0.305mV/LSB across 5V, which is more than enough resolution for FFT spectral analysis.

**No built-in OP AMP:** The R4 WiFi's built-in OP AMP (pins A1/A2/A3) is a signal-level amplifier. The headphone signal is already strong enough for the 14-bit ADC — amplification is unnecessary. The OP AMP pins are reserved for potential mic pre-amplification in ADR-0004.

**No buffer needed:** The conditioning circuit presents ~50kΩ input impedance. Combined with the Billy Bass amp input (typically 10–47kΩ), the total Y-splitter load remains far above the BT adapter's designed 16–32Ω headphone drive capability. No audible signal degradation on the speaker side.

## Bill of Materials

| Item | ~Cost |
|------|-------|
| Bluetooth 5.0 audio receiver (3.5mm output) | $5–10 |
| 3.5mm Y-splitter cable | $3 |
| C1: 1µF film capacitor | $0.10 |
| R1, R2: 100kΩ resistors ×2 | $0.10 |
| D1, D2: BAT43 Schottky diodes ×2 | $0.20 |
| R3: 3.3kΩ resistor | $0.05 |
| C2: 10nF ceramic capacitor | $0.05 |
| **Total** | **~$9–14** |

## Implementation Notes

- BT adapter pairs as a named Bluetooth device; phone/smart speaker connects to it
- BT adapter needs USB power — can share the same 5V supply as the Arduino or use a separate USB power source
- Y-splitter is a standard 3.5mm 1-male-to-2-female cable
- Conditioning circuit can be built on a small piece of perfboard or soldered directly to wires
- 3.5mm sleeve (ground) from the Y-splitter must connect to Arduino GND for a shared ground reference
- In firmware: use `analogReadResolution(14)` for full 14-bit range (0–16383), subtract ~8192 DC bias before FFT

## Consequences

- Bluetooth audio capability restored via external adapter despite R4 WiFi lacking A2DP
- No new speaker or amplifier needed — reuses existing Billy Bass audio hardware
- Clean analog audio signal directly to ADC — better FFT input than mic pickup in noisy environments
- BT adapter is an external component that needs its own power and physical placement near the fish
- Phone/smart speaker BT volume directly controls signal amplitude into the ADC — volume too high clips (diodes protect), too low gives weak FFT readings
- Circuit pinout diagram needs updating to show the conditioning circuit on A0
- Motor brush noise may couple into the 5V bias rail — add a 10µF electrolytic cap from 5V to GND near R1/R2 to stabilize; add 0.1µF ceramic caps across each motor terminal to suppress brush noise
- Ground loop hum is possible if BT adapter and Arduino use different power sources — shared ground via 3.5mm sleeve mitigates this
