# fpv_sl — FPV Sound Logger

An embedded audio recorder for FPV drones, running on a **Raspberry Pi Pico (RP2040)**.
Captures audio from an I2S MEMS microphone, applies a digital high-pass filter to cut motor noise, and writes WAV files directly to a SD card.

---

## Overview

The module plugs into the drone's power rail and flight controller GPIO. It records audio automatically based on the configured trigger mode, stores files on a SD card, and exposes the card as a USB mass storage device when connected to a computer for easy file retrieval.

---

## Hardware

| Component | Description |
|---|---|
| MCU | Raspberry Pi Pico (RP2040, dual-core Cortex-M0+ @ 133 MHz) |
| Microphone | INMP441 — I2S MEMS microphone |
| Storage | SD card via SPI (FatFS) |
| Status LED | WS2812 RGB (production) / onboard GP25 (debug) |
| FC interface | 2× GPIO inputs from Flight Controller |

### Pin mapping

| Signal | GPIO | Note |
|---|---|---|
| I2S MIC DATA (SD) | 26 | |
| I2S MIC CLOCK (SCK) | 27 | |
| I2S MIC WORD SELECT (WS) | 28 | |
| SPI SD SCK | 10 | |
| SPI SD MOSI | 11 | |
| SPI SD MISO | 12 | |
| SPI SD CS | 13 | |
| FC ENABLE pin | 1 | Arm/disarm input from FC |
| FC RECORD pin | 2 | Record trigger from FC |
| WS2812 LED | 16 | Production RGB LED |
| Onboard LED | 25 | Debug mode (see build options) |

> Default pins can be overridden by defining `USE_CUSTOM_BOARD_PINS` and the corresponding `PIN_*` macros in `modules/fpv_sl_core_board_pin.h`.

---

## Audio pipeline

The recording pipeline is split across both RP2040 cores to sustain continuous throughput:

```
INMP441 ──I2S──► DMA (ping-pong) ──► Ring buffer (8 × 256 samples)
                                            │
                          ┌─────────────────┴──────────────────┐
                          │ Core 0                             │ Core 1
                          │ - Picks a ready block              │ - Receives block via FIFO
                          │ - Sends ptr to Core 1              │ - Applies DSP chain
                          │ - Writes filtered block to SD      │ - Sends ptr back to Core 0
                          └────────────────────────────────────┘
                                            │
                                     SD card (WAV)
```

- **Sample rates**: 22 080 Hz or 44 180 Hz (configurable)
- **Channels**: mono or stereo (configurable)
- **Bit depth**: 32-bit PCM (INMP441 outputs 24-bit I2S data stored as int32)
- **Buffer**: ring buffer of 8 blocks × 256 samples × 4 bytes = ~8 kB, providing ~128 ms of margin at 16 kHz

### DSP chain (Core 1, per sample)

```
raw sample (32-bit)
    │
    ▼
  >> 8   (24-bit alignment)
    │
    ▼
[High-pass IIR]  ──  y[n] = α·(y[n-1] + x[n] - x[n-1])     α = fs / (fs + 2π·fc_hp)
    │                enabled by use_high_pass_filter, fc set by high_pass_cutoff_freq
    ▼
[Low-pass IIR]   ──  y[n] = α·x[n] + (1-α)·y[n-1]           α = 2π·fc_lp / (fs + 2π·fc_lp)
    │                enabled by use_low_pass_filter, fc set by low_pass_cutoff_freq
    ▼
  × gain          ──  gain = mic_gain / 100   (e.g. 80 → 0.8×, 100 → 1.0×)
    │
    ▼
 int32_t output
```

Both filters are 1st-order IIR. Each can be independently enabled or disabled. When both are active they form a **band-pass filter** (default passband: ~200 Hz – 8 kHz), which removes low-frequency motor rumble below the HP cutoff and high-frequency noise above the LP cutoff.

---

## Recording modes

Configured via `default.conf` on the SD card root:

| Mode | Trigger | Description |
|---|---|---|
| `RECORD_ON_BOOT` | Power-on | Starts recording immediately when powered |
| `RCD_ONLY` | ARM pin | Waits for the FC ARM signal to start recording |
| `CLASSIC` | ENABLE then ARM | Standby until ENABLE, then records on ARM |

> **Note — RCD_ONLY and the triple-trigger delete feature:** In `RCD_ONLY` mode, only the ARM/RECORD pin is strictly required for recording. However, if you want to use the **triple-trigger ENABLE delete feature** (see below), you must also wire the ENABLE pin to the FC (e.g., via Betaflight PinioBox or equivalent). Without this wire, triple-trigger detection is not possible in `RCD_ONLY` mode.

---

## Triple-trigger ENABLE — delete all recordings

Toggle the ENABLE pin (or MSP arm signal) **3 times within 5 seconds** while **not recording** to delete all WAV files in the recording folder and reset the file index to 0. This is a quick in-field cleanup without needing to connect USB or a computer.

- Only active while **not recording** — the counter is ignored during an active recording session.
- Each toggle = one rising edge on the ENABLE pin (GPIO) or one activation of the configured MSP ENABLE channel.
- If more than 5 seconds pass between two edges, the counter resets.
- The LED shows **3 rapid short flashes** (debug) or **solid red** (production) while files are being deleted, then returns to the ready state.
- Controlled by the `delete_on_triple_arm` config key.

**Mode availability:**

| Mode | Triple-trigger available |
|---|---|
| `RECORD_ON_BOOT` (GPIO) | Never — no ENABLE pin in this mode |
| `RECORD_ON_BOOT` (MSP) | **USB-powered phase only** (before LiPo connection) — this is the only window where deletion is possible in this mode |
| `RCD_ONLY` | Yes, in idle (between recordings) |
| `CLASSIC` | Yes, in idle (ENABLE active but not yet armed) |

> **`RECORD_ON_BOOT` + MSP:** when the FC is powered by USB only (no LiPo, `vbat < msp_lipo_min_mv`), recording is held. This USB-powered phase — typically used for pre-flight GPS lock — is the **only opportunity** to trigger a file deletion before the flight starts. Once the LiPo is connected, recording begins immediately and triple-trigger is no longer checked.

---

## MSP interface (Betaflight / iNAV)

When `use_uart_msp = true`, the module polls the FC over UART using the **MSP v2** protocol (`$X` framing, CRC8 DVB-S2) instead of (or alongside) the GPIO pins.

**Polling rate:** 30 Hz, both in idle and recording modes.

**Messages polled each cycle:**

| MSP message | Function ID | Used for |
|---|---|---|
| `MSP_STATUS` | 101 | ARM flag → `on_record` / `on_disarm` callbacks |
| `MSP_RC` | 105 | ENABLE channel value → `on_enable` / `on_disable` + triple-trigger |
| `MSP_ANALOG` | 110 | Battery voltage → LiPo detection (see below) |

**ENABLE channel:** configure `msp_enable_channel` (default `5` = AUX1) and the active range (`msp_channel_range_min` / `msp_channel_range_max`, default `1700`–`2100` µs). This works like the Betaflight mode sliders — the channel is considered active when its value falls within the range, allowing use of any position of a 2- or 3-position switch.

**ARM signal:** detected via `MSP_STATUS.armed` flag — no channel config needed.

**LiPo detection (`RECORD_ON_BOOT` mode):** when the measured battery voltage is below `msp_lipo_min_mv` (default `3000` mV), the module assumes it is USB-powered only (e.g., pre-flight GPS lock phase). In this state, recording does not start, but the triple-trigger delete feature remains active for in-field cleanup before the flight.

**Wiring:** two wires required — MSP is a request/response protocol, the Pico initiates every query. Connect FC UART TX → Pico UART RX, and Pico UART TX → FC UART RX. Enable `MSP` on the corresponding FC port in Betaflight/iNAV Ports tab.

---

## USB

When connected to a computer, the module enumerates as a **USB Mass Storage (MSC)** device, exposing the SD card contents. A **CDC** interface is also available for debug logging over serial.

Enumeration is attempted for 3 seconds at boot. If no host is detected, the module switches directly to the recording loop.

---

## Status indicator

### Production — RGB WS2812 (GP16)

| State | Color | Mode |
|---|---|---|
| Powered / boot | White | Fixed |
| USB MSC connected | Blue | Fixed |
| USB data transfer | Blue | Blink |
| Ready to record | Green | Fixed |
| Recording | Green | Blink |
| Disk alert (>80%) | Orange | Blink |
| Disk critical (>95%) | Red | Blink |
| Flushing audio files | Red | Fixed |

### Debug — Onboard LED (GP25)

Single LED with blink patterns (active with `FPV_SL_PICO_PROBE_DEBUG=ON`):

| State | Pattern |
|---|---|
| Powered / boot | Fixed ON |
| USB MSC connected | Slow blink — 500 ms |
| USB data transfer | Fast blink — 100 ms |
| Ready to record | Double flash repeated |
| Recording | Triple flash repeated |
| Disk alert (>80%) | Quadruple flash repeated |
| Disk critical (>95%) | Very fast blink — 50 ms |
| Flushing audio files | 3 rapid short flashes repeated |

---

## Configuration file

`default.conf` must be placed at the root of the SD card. Key-value format, one entry per line.

| Key | Values | Description |
|---|---|---|
| `use_enable_pin` | `true` / `false` | Enable the FC ENABLE pin trigger |
| `record_on_boot` | `true` / `false` | Record immediately on power-on |
| `mic_gain` | `0` – `200` | Output gain in percent — `80` = 0.8×, `100` = 1.0× (unity), `200` = 2.0×. Default `80`. |
| `use_high_pass_filter` | `true` / `false` | Enable the high-pass filter (removes motor rumble below the cutoff). Default `true`. |
| `high_pass_cutoff_freq` | `1` – `255` | High-pass cutoff frequency in Hz. Default `200`. |
| `use_low_pass_filter` | `true` / `false` | Enable the low-pass filter (removes high-frequency noise above the cutoff). Default `true`. |
| `low_pass_cutoff_freq` | `1` – `22000` | Low-pass cutoff frequency in Hz. Default `8000`. |
| `sample_rate` | `22080` / `44180` | I2S sample rate (Hz) |
| `mono_record` | `true` / `false` | Record in mono (single channel) |
| `file_index` | `1` – `...` | Auto-incremented index for unique file names |
| `record_folder` | `/` / `records/` | Destination folder for WAV files |
| `record_prefix` | `mic_wav` / ... | Base name for WAV files |
| `delete_on_triple_arm` | `true` / `false` | Enable the triple-trigger ENABLE delete feature (see below) |
| `max_record_duration` | seconds (default `300`) | Maximum recording duration used for WAV pre-allocation via `f_expand()`. Falls back gracefully if contiguous space is unavailable. |
| `use_uart_msp` | `true` / `false` | Enable MSP polling over UART to detect FC arm state and trigger recording (replaces or complements GPIO pins) |
| `msp_uart_id` | `0` / `1` | Pico UART peripheral to use for MSP (default `1`) |
| `msp_baud_rate` | e.g. `115200`, `230400`, `460800` | Baud rate for the MSP UART (default `115200`) |
| `msp_enable_channel` | `1` – `16` | RC channel number (1-based) used as the ENABLE signal via MSP. Maps to AUX1=5, AUX2=6, etc. Used for CLASSIC mode gating and triple-trigger delete. Default `5`. |
| `msp_channel_range_min` | `1000` – `2000` | Lower bound (µs) of the active range for the ENABLE channel, like the Betaflight mode slider. Default `1700`. |
| `msp_channel_range_max` | `1000` – `2000` | Upper bound (µs) of the active range for the ENABLE channel. Default `2100`. |
| `msp_lipo_min_mv` | mV (default `3000`) | Minimum battery voltage (mV) to consider a LiPo connected. Below this threshold (USB-only power), `RECORD_ON_BOOT` mode holds recording until LiPo is detected, while still allowing triple-trigger delete. |
| `telemetry_items` | bitmask (default `1`) | MSP data sources to record alongside audio. Sum the values of the desired sources: `1`=RC sticks, `2`=attitude, `4`=GPS, `8`=analog. Examples: `1`=sticks only, `3`=sticks+attitude, `15`=all. `0` disables telemetry. Only active when `use_uart_msp = true`. |

---

## Telemetry (.tlm files)

When `use_uart_msp = true` and `telemetry_items > 0`, the module records a `.tlm` file alongside each WAV file. Both share the same index and prefix (e.g., `rec5.wav` + `rec5.tlm`).

### File format

```
[Header — 8 bytes]
  magic[4]        : "FPVT"
  version         : uint8  (1)
  items           : uint8  (bitmask of recorded sources)
  source_protocol : uint8  (1 = MSP, 2 = MAVLink)
  sample_rate_hz  : uint8  (polling rate, e.g. 30)

[Records — repeated, same layout for the entire session]
  timestamp_ms    : uint32  (ms since boot — always present)
  channels[8]     : uint16 × 8   (CH1–CH8 in µs)   if items & 1
  roll            : int16         (deci-degrees)     if items & 2
  pitch           : int16         (deci-degrees)     if items & 2
  yaw             : int16         (degrees)          if items & 2
  gps_fix         : uint8                            if items & 4
  gps_sats        : uint8                            if items & 4
  lat             : int32         (degE7)            if items & 4
  lon             : int32         (degE7)            if items & 4
  alt             : uint16        (metres)           if items & 4
  speed           : uint16        (cm/s)             if items & 4
  vbat_mv         : uint16        (millivolts)       if items & 8
  mah             : uint16        (mAh drawn)        if items & 8
  rssi            : uint16        (0–1023)           if items & 8
  current_ca      : int16         (centi-amps)       if items & 8
```

All values are little-endian. Record size is fixed for a given session (determined by the `items` bitmask in the header). Default `items = 1`: record = 20 bytes (4 + 16).

### Correlating audio and telemetry

Both files start at the same moment (recording ARM). Use `timestamp_ms` to align a telemetry record with an audio sample:

```python
import struct, sys

with open("rec5.tlm", "rb") as f:
    magic, version, items, proto, rate = struct.unpack("4sBBBB", f.read(8))
    assert magic == b"FPVT"

    while True:
        chunk = f.read(4)
        if not chunk: break
        ts_ms = struct.unpack("<I", chunk)[0]
        # read remaining fields based on items bitmask
        if items & 1: channels = struct.unpack("<8H", f.read(16))
        if items & 2: roll, pitch, yaw = struct.unpack("<3h", f.read(6))
        # ...
        audio_sample = int(ts_ms / 1000 * 44100)  # corresponding WAV sample index
        print(f"t={ts_ms}ms  ch1={channels[0]}µs  sample={audio_sample}")
```

---

## Build

### Prerequisites

- CMake ≥ 3.13
- Ninja
- arm-none-eabi-gcc (toolchain 14.2)
- Pico SDK 2.2.0

### Commands

```bash
# Check environment
cmake --version
ninja --version
arm-none-eabi-gcc --version

# Clean
rm -rf src/build

# Configure (production)
cmake -S src -B src/build -G Ninja

# Configure (debug — Pi Pico standard + probe)
cmake -DFPV_SL_PICO_PROBE_DEBUG=ON -S src -B src/build -G Ninja

# Build
cmake --build src/build
```

### CMake options

| Option | Default | Description |
|---|---|---|
| `FPV_SL_PICO_PROBE_DEBUG` | `OFF` | Use the onboard LED (GP25) with blink patterns instead of the WS2812 RGB LED. For debug sessions with a standard Pi Pico and a debug probe. |

---

## Testing

Two test levels are available: **host tests** (PC, no hardware required) and an **on-target test runner** (Pico firmware). See [TESTING.md](TESTING.md) for the full step-by-step guide.

```bash
# Host tests — quick check, no hardware needed
cmake -S src/tests -B src/tests/build_host
cmake --build src/tests/build_host
ctest --test-dir src/tests/build_host -V
```

---

## Project structure

```
fpv_sl/
└── src/
    ├── config/             # Config file parser (default.conf)
    ├── drivers/
    │   ├── i2s_mic/        # I2S DMA driver for INMP441
    │   ├── ws2812/         # WS2812 RGB LED driver (PIO)
    │   └── no-OS-FatFS-*/  # FatFS SPI SD library (third-party)
    ├── modules/
    │   ├── audio_buffer/   # Lock-free ring buffer (dual-core pipeline)
    │   ├── gpio/           # FC GPIO interface (ENABLE / RECORD pins)
    │   ├── sdio/           # SD file helpers (WAV creation, config read)
    │   └── status_indicator/ # LED abstraction (RGB or onboard)
    ├── usb/                # TinyUSB MSC + CDC descriptors
    ├── utils/              # Logging, string cast helpers
    └── fpv_sl_loader.c     # Main entry point
```
