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
| FC interface | 2× GPIO inputs (GPIO mode) or UART bidirectionnel MSP/MAVLink |

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
| FC ENABLE pin | 1 | GPIO mode — arm/disarm input from FC |
| FC RECORD pin | 2 | GPIO mode — record trigger from FC |
| MSP UART TX | 4 | MSP mode (`use_uart_msp = true`) — connect to FC UART RX |
| MSP UART RX | 5 | MSP mode — connect to FC UART TX |
| WS2812 LED | 16 | Production RGB LED |
| Onboard LED | 25 | Debug mode (see build options) |

> GPIO pins 1/2 and UART pins 4/5 are defaults. Both can be remapped via `USE_CUSTOM_BOARD_PINS` and the corresponding `PIN_*` macros in `modules/fpv_sl_core_board_pin.h`.
>
> In `FPV_SL_PICO_PROBE_DEBUG` mode, FC GPIO pins are remapped to GP2/GP3 (GP1/GP2 are used by the SWD debug probe).

---

## Audio pipeline

The recording pipeline is split across both RP2040 cores to sustain continuous throughput:

```
INMP441 ──I2S──► DMA (ring buffer, canal unique relancé par IRQ) ──► audio_pipeline (8 × 256 samples)
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
- **Bit depth**: 32-bit PCM (INMP441 outputs 24-bit I2S data in 32-bit DMA word; `process_sample` aligns to 24 bits ±2²³, `write_buffer` writes `int32_t` directly — full precision preserved)
- **Buffer**: ring buffer of 8 blocks × 256 samples × 4 bytes = ~8 kB, providing ~46 ms of margin at 22 050 Hz

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

### Telemetry pipeline (Core 0, MSP / MAVLink)

When `use_uart_msp = true`, Core 0 interleaves telemetry polling with its audio write cycle at 30 Hz:

```
FC (Betaflight / iNAV)
    │  UART TX/RX (MSP v2 / MAVLink)
    ▼
[Core 0 — 30 Hz poll]
    │
    ├── MSP_STATUS  (101) ──► ARM flag   → on_record / on_disarm
    ├── MSP_RC      (105) ──► CH1–CH8   → on_enable / on_disable + triple-trigger
    ├── MSP_ATTITUDE(108) ──► roll/pitch/yaw         ┐
    ├── MSP_RAW_GPS (106) ──► fix/sats/lat/lon/alt   ├─ if telemetry_items > 0
    └── MSP_ANALOG  (110) ──► vbat/mAh/rssi/current  ┘
              │
              ▼
    [msp_get_telemetry_record()]
    packs active fields into a binary record (4–47 B, layout from items bitmask)
              │
              ▼
    [tlm_writer_write()]   ──►  SD card (.tlm file)
    f_sync every 300 records (~10 s)
```

Each record is timestamped (`uint32_t ms since boot`) and only contains the fields enabled by `telemetry_items`. Record size is fixed for a given session and described in the file header.

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

## MAVLink interface (ArduPilot / iNAV)

> **Not yet implemented.** Planned for a future release.

The MAVLink interface will use the same UART (GP4/GP5) with auto-detection of the framing (`$X` for MSP v2, `0xFE`/`0xFD` for MAVLink v1/v2). The same four callbacks (`on_enable`, `on_disable`, `on_record`, `on_disarm`) and telemetry struct will be populated from MAVLink messages (`HEARTBEAT`, `RC_CHANNELS`, `ATTITUDE`, `GPS_RAW_INT`, `SYS_STATUS`), keeping the recording state machine and `.tlm` format unchanged.

---

## USB

When connected to a computer, the module enumerates as a **USB Mass Storage (MSC)** device, exposing the SD card contents. A **CDC** interface is also available for debug logging over serial.

Enumeration is attempted for 3 seconds at boot. If no host is detected, the module switches directly to the recording loop.

### CDC Simulator (`FPV_SL_CDC_SIM`)

Build option that replaces the MSC mode with a **recording mode driven from the PC over USB CDC**. Useful for testing the state machine (ENABLE / ARM / DISARM) without any FC wiring.

```bash
# Build with CDC simulator
cmake -DFPV_SL_CDC_SIM=ON -S src -B src/build -G Ninja
cmake --build src/build
```

Once flashed, connect to the CDC serial port (115200 baud) and send 2-byte commands:

| Command | Effect |
|---|---|
| `e1` | ENABLE (equivalent to switch up) |
| `e0` | DISABLE |
| `r1` | ARM (start recording) |
| `r0` | DISARM (stop recording) |

```bash
# Example with picocom (Linux/macOS)
picocom -b 115200 /dev/ttyACM0
# then type: r1   (start)   r0   (stop)

# Example with Python
python -c "import serial,time; s=serial.Serial('/dev/ttyACM0',115200); s.write(b'r1'); time.sleep(5); s.write(b'r0')"
```

> **Note:** the SD card remains mounted (FatFS active) — WAV and TLM files are written normally. The triple-trigger delete sequence (`e1e1e1` sent within 5 s) also works in this mode.

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

Both files start at the same moment (recording ARM). Use `timestamp_ms` (ms since boot, `uint32_t`, little-endian) to align a telemetry record with an audio sample:

```
audio_sample_index = timestamp_ms / 1000 × sample_rate
```

The header `items` bitmask describes the exact layout of each record, so a parser can seek directly to any record by index (`offset = 8 + record_index × tlm_record_size(items)`).

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

# bash clean
rm -rf src/build
# powershell clean
Remove-Item -Recurse -Force build

# Configure (production)
cmake -S src -B src/build -G Ninja

# Configure (debug — Pi Pico standard + probe)
cmake -DFPV_SL_PICO_PROBE_DEBUG=ON -S src -B src/build -G Ninja

# Configure (CDC simulator — test state machine via USB without FC)
cmake -DFPV_SL_CDC_SIM=ON -S src -B src/build -G Ninja

# Build
cmake --build src/build
```

### CMake options

| Option | Default | Description |
|---|---|---|
| `FPV_SL_PICO_PROBE_DEBUG` | `OFF` | Use the onboard LED (GP25) with blink patterns instead of the WS2812 RGB LED. For debug sessions with a standard Pi Pico and a debug probe. |
| `FPV_SL_CDC_SIM` | `OFF` | CDC simulator: skip MSC mode, run recording loop with USB+CDC active. Send 2-byte commands (`e1`/`e0`/`r1`/`r0`) to trigger ENABLE/DISABLE/ARM/DISARM. See [USB › CDC Simulator](#cdc-simulator-fpv_sl_cdc_sim). |

### Step-by-step debugging (picoprobe + OpenOCD)

#### Requirements

- A Raspberry Pi Pico **probe** flashed with [picoprobe](https://github.com/raspberrypi/picoprobe/releases) (`picoprobe.uf2`)
- A Raspberry Pi Pico **target** (the fpv_sl module)
- VS Code extension: **Cortex-Debug** (`marus25.cortex-debug`)

#### Wiring — probe → target

| Probe (GP) | Target | Function |
|---|---|---|
| GND | GND | Common ground — mandatory |
| GP2 | SWDIO | SWD data |
| GP3 | SWDCLK | SWD clock |
| GP8 | GP2 | FC ENABLE simulator (debug mode) |
| GP9 | GP3 | FC RECORD simulator (debug mode) |

> In `FPV_SL_PICO_PROBE_DEBUG` mode, FC ENABLE/RECORD pins are remapped to GP2/GP3 (instead of GP1/GP2 in production) to avoid conflict with the SWD wires. The GP8/GP9 simulator outputs on the probe side allow triggering ENABLE and RECORD without real FC wiring (required for the on-target test runner).

#### Build

```bash
cmake -DFPV_SL_PICO_PROBE_DEBUG=ON -S src -B src/build -G Ninja
cmake --build src/build
# flash src/build/fpv_sl_loader.uf2 onto the target
```

#### Start OpenOCD

Run in a dedicated terminal and leave it running:

```bash
# Linux / macOS
~/.pico-sdk/openocd/0.12.0+dev/openocd \
  -s ~/.pico-sdk/openocd/0.12.0+dev/scripts \
  -f interface/cmsis-dap.cfg \
  -f target/rp2040.cfg \
  -c "adapter speed 5000"
```

```powershell
# Windows (PowerShell)
& "$env:USERPROFILE\.pico-sdk\openocd\0.12.0+dev\openocd.exe" `
  -s "$env:USERPROFILE\.pico-sdk\openocd\0.12.0+dev\scripts" `
  -f interface/cmsis-dap.cfg `
  -f target/rp2040.cfg `
  -c "adapter speed 5000"
```

Listens on `localhost:3333` (GDB) and `localhost:4444` (telnet).

#### GDB — command line

```bash
# Linux / macOS
~/.pico-sdk/toolchain/14_2_Rel1/bin/arm-none-eabi-gdb src/build/fpv_sl_loader.elf
```

```powershell
# Windows
& "$env:USERPROFILE\.pico-sdk\toolchain\14_2_Rel1\bin\arm-none-eabi-gdb.exe" src/build/fpv_sl_loader.elf
```

```
(gdb) target remote localhost:3333
(gdb) monitor reset init
(gdb) break main
(gdb) continue
```

#### VS Code — F5

`.vscode/launch.json` is provided in the repo. Open the project at its root, select the **fpv_sl — OpenOCD (debug probe)** configuration and press **F5**. OpenOCD is started automatically by Cortex-Debug; execution stops at `main()`.

> Paths in `launch.json` use `${env:USERPROFILE}` and point to `~/.pico-sdk/` — the standard installation path used by the Pico SDK VS Code extension.

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
    ├── config/                  # default.conf parser, fpv_sl_conf_t definition
    ├── drivers/
    │   ├── i2s_mic/             # I2S DMA driver for INMP441 (ping-pong DMA, overrun detection)
    │   ├── msp/                 # MSP v2 protocol driver (UART, state-machine parser, CRC8)
    │   ├── ws2812/              # WS2812 RGB LED driver (PIO)
    │   └── no-OS-FatFS-*/       # FatFS over SPI SD (third-party submodule)
    ├── modules/
    │   ├── audio_buffer/        # Lock-free ring buffer (8 × 256 samples, dual-core pipeline)
    │   ├── gpio/                # FC GPIO interface (ENABLE / RECORD IRQ callbacks)
    │   ├── msp/                 # MSP business logic — 30 Hz polling, ARM/ENABLE detection, telemetry packing
    │   ├── sdio/                # FatFS helpers — WAV lifecycle, config read/write, disk usage
    │   ├── status_indicator/    # LED abstraction (WS2812 RGB or onboard GP25 blink patterns)
    │   ├── telemetry/           # .tlm file writer (dynamic binary format, f_sync checkpointing)
    │   ├── fpv_sl_core.c/h      # Dual-core recording loop, DSP filter chain, mode state machine
    │   └── fpv_sl_core_board_pin.h  # Default GPIO pin assignments (overridable)
    ├── tests/
    │   ├── host/                # Unity host tests (PC, no hardware)
    │   │   ├── test_cast_from_str.c
    │   │   ├── test_audio_buffer.c
    │   │   ├── test_config_parser.c
    │   │   ├── test_disk_usage.c
    │   │   ├── test_dsp_filter.c
    │   │   ├── test_recording_mode.c
    │   │   └── test_tlm_packing.c
    │   ├── target/              # On-target test runner firmware (Pico, USB CDC report)
    │   └── stubs/               # Pico SDK stubs for host compilation (ff.h, pico/, pico_stubs.c)
    ├── usb/                     # TinyUSB MSC + CDC descriptors and disk backend
    ├── utils/                   # debug_log.h (LOGI/LOGW/LOGE/LOGD), cast_from_str
    └── fpv_sl_loader.c          # main() — boot, USB enumeration, mode dispatch, CDC SIM
```
