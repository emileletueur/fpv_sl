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
                          │ - Sends ptr to Core 1              │ - Applies high-pass filter
                          │ - Writes filtered block to SD      │ - Sends ptr back to Core 0
                          └────────────────────────────────────┘
                                            │
                                     SD card (WAV)
```

- **Sample rates**: 22 080 Hz or 44 180 Hz (configurable)
- **Channels**: mono or stereo (configurable)
- **DSP**: 1st-order IIR high-pass filter (α ≈ 0.959, cutoff ~300 Hz) — removes low-frequency motor noise
- **Buffer**: ring buffer of 8 blocks × 256 samples × 4 bytes = ~8 kB, providing ~128 ms of margin at 16 kHz

---

## Recording modes

Configured via `default.conf` on the SD card root:

| Mode | Trigger | Description |
|---|---|---|
| `ALWAYS_RCD` | Power-on | Starts recording immediately when powered |
| `RCD_ONLY` | ARM pin | Waits for the FC ARM signal to start recording |
| `CLASSIC` | ENABLE then ARM | Standby until ENABLE, then records on ARM |

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

---

## Configuration file

`default.conf` must be placed at the root of the SD card. Key-value format, one entry per line.

| Key | Values | Description |
|---|---|---|
| `use_enable_pin` | `true` / `false` | Enable the FC ENABLE pin trigger |
| `always_rcd` | `true` / `false` | Record immediately on power-on |
| `mic_gain` | `10` – `110` | Microphone gain factor (100 = unity) |
| `use_high_pass_filter` | `true` / `false` | Enable the digital high-pass filter |
| `high_pass_cutoff_freq` | `50` – `500` | High-pass filter cutoff frequency (Hz) |
| `sample_rate` | `22080` / `44180` | I2S sample rate (Hz) |
| `is_mono_rcd` | `true` / `false` | Record in mono (single channel) |
| `next_file_name_index` | `1` – `...` | Auto-incremented index for unique file names |
| `rcd_folder` | `/` / `records/` | Destination folder for WAV files |
| `rcd_file_name` | `mic_wav` / ... | Base name for WAV files |
| `del_on_multiple_enable_tick` | `true` / `false` | Delete all WAV files in the recording folder if ENABLE is toggled 3× within 5 s (only effective while disarmed) |

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
