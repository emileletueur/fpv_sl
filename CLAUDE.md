# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project overview

**fpv_sl** is an embedded firmware for a Raspberry Pi Pico (RP2040) that records audio from an INMP441 I2S MEMS microphone to a SD card as WAV files. It targets FPV drones, filtering out motor noise via a digital high-pass filter.

## Build commands

All commands run from the `src/` directory (or use `src/build` as the build directory from repo root):

```bash
# Configure — production (WS2812 RGB LED)
cmake -S src -B src/build -G Ninja

# Configure — debug (onboard LED GP25 + picoprobe)
cmake -DFPV_SL_PICO_PROBE_DEBUG=ON -S src -B src/build -G Ninja

# Build
cmake --build src/build

# Clean
rm -rf src/build
```

The output binary is `src/build/fpv_sl_loader.uf2` (and `.elf`).

**Toolchain requirements:** CMake ≥ 3.13, Ninja, arm-none-eabi-gcc 14.2, Pico SDK 2.2.0.

**`FPV_SL_PICO_PROBE_DEBUG=ON` émet deux define distincts :**
- `USE_PICO_ONBOARD_LED` — LED type (onboard GP25 au lieu de WS2812)
- `FPV_SL_PICO_PROBE_DEBUG` — GPIO FC remappés GP2/GP3 (SWD sur GP1/GP2 en mode probe) + sorties simulateur FC GP8/GP9

## Conventions

- **Commits** : pas de mention "Claude" ni de ligne `Co-Authored-By` dans les messages de commit.
- Messages courts, impératifs, français ou anglais (les deux sont utilisés dans ce repo).
- **À chaque implémentation** : mettre à jour ce fichier si l'architecture ou les conventions évoluent (section concernée + TODO), et ajouter ou mettre à jour les tests host Unity si la logique est testable sur PC.

### Architecture — règle d'orchestration

`fpv_sl_core` est le seul module autorisé à appeler les autres modules. Les modules bas niveau (`sdio`, `gpio`, `audio_buffer`, `status_indicator`, etc.) ne doivent **jamais** s'appeler entre eux — pas de dépendances inter-modules.

### Logging

Toute nouvelle fonction doit tracer ses points clés avec les macros de `debug_log.h` :

| Macro | Usage |
|---|---|
| `LOGI` | Début d'opération, résultat nominal, valeurs utiles au diagnostic |
| `LOGW` | Situation anormale mais non bloquante |
| `LOGE` | Erreur : code retour FatFS, pointeur NULL, état incohérent |
| `LOGD` | Détail verbeux utile en debug poussé uniquement |

Règle : un chemin d'erreur sans `LOGE` est un bug de traçabilité.

## Code style

Format: clang-format with `src/clang-format` (LLVM style, 120-column limit, 4-space indent, no tabs, `PointerAlignment: Right`).

## Architecture

### Dual-core audio pipeline

The core design splits work across both RP2040 cores:

- **Core 0** (`fpv_sl_core0_loop`): checks the ring buffer (`audio_pipeline_t`), pushes a ready block pointer to Core 1 via `multicore_fifo_push_blocking`, waits for the filtered pointer back, then writes to SD via `file_helper`.
- **Core 1** (`fpv_sl_core1_loop`): receives a block pointer, applies the 1st-order IIR high-pass filter (`process_sample` / `hp_filter_t`), optionally compacts stereo→mono, then pushes the pointer back to Core 0.
- **DMA** fills the ring buffer (`audio_pipeline_t` in `modules/audio_buffer/`) using ping-pong DMA channels, independent of both cores.

### Boot sequence (`fpv_sl_loader.c`)

1. TinyUSB initialized; waits up to 3 s for USB host enumeration.
2. If USB host detected → USB MSC + CDC loop (exposes SD card as mass storage; config/recording only start after CDC ready at 5 s).
3. If USB timeout → recording mode directly.

### WAV file lifecycle

`t_mic_rcd.wav` is the sentinel file for in-progress recordings.

| Phase | What happens |
|---|---|
| `create_wav_file()` | Opens `t_mic_rcd.wav`, calls `f_expand()` to pre-allocate a contiguous block (`sample_rate × channels × 2 × MAX_RECORD_DURATION + 44` bytes), writes a placeholder WAV header with `data_bytes = 0`. If `f_expand` fails (fragmented card, insufficient contiguous space), recording continues without pre-allocation — `LOGW` emitted, no abort. |
| `write_buffer()` + `sync_wav_file()` | Every `SYNC_PERIOD_BLOCKS` (64) blocks written (~370 ms at 44.1 kHz), `sync_wav_file()` rewrites the WAV header at offset 0 with the current `g_audio_bytes_written` value (checkpoint), seeks back to the write position, and calls `f_sync()`. |
| `finalize_wav_file()` | If pre-allocation was active: `f_truncate()` at `sizeof(wav_header_t) + g_audio_bytes_written` to release unused clusters. Then rewrites the final WAV header, closes, renames to `<record_folder><record_prefix><index>.wav`, increments `FILE_INDEX` in `default.conf`. `f_truncate` failure is non-fatal (LOGW) — audio content is unaffected. |
| `recover_unfinalized_recording()` | Called at boot before USB/recording split. Detects `t_mic_rcd.wav` via `f_stat`. Reads the existing WAV header: if `data_bytes > 0` and within file bounds, uses it as the valid audio size (accurate to the last sync, ≤ 370 ms loss). If `data_bytes == 0` (power cut before first sync), falls back to `finfo.fsize - 44` (may include uninitialized pre-allocated data at tail). In both cases, `f_truncate()` is called before rename. |

**Power-cut guarantees:**
- Normal recording (no power cut): exact file size, clean close.
- Power cut after ≥ 1 sync: recovered file accurate to within ~370 ms.
- Power cut before first sync (< 370 ms after start): fallback recovery using full file size — tail may contain uninitialized SD sector data if pre-allocation was active.

### Configuration

`default.conf` on the SD card root is parsed at runtime by `config/fpv_sl_config.{h,c}` and `modules/sdio/file_helper.{c,h}`. The parsed result is `fpv_sl_conf_t`. Recording mode (`execution_condition_t`) is derived from `record_on_boot` and `use_enable_pin` flags.

If `default.conf` is absent, factory defaults are applied and the file is created on the SD card (`FR_NO_FILE` path in `read_conf_file()`). If the SD card fails to mount, `conf_is_loaded` stays false and recording does not start.

### Key modules

| Path | Role |
|---|---|
| `fpv_sl_loader.c` | `main()` — boot, USB enumeration, mode dispatch |
| `modules/fpv_sl_core.{c,h}` | Dual-core recording loop, DSP filter, mode state machine |
| `modules/audio_buffer/` | Lock-free ring buffer (8 × 256 int32 samples) shared by DMA/Core0/Core1 |
| `drivers/i2s_mic/` | I2S DMA driver for INMP441; exposes `init_i2s_mic`, `i2s_mic_start/stop` |
| `drivers/ws2812/` | WS2812 PIO driver (production LED) |
| `modules/status_indicator/` | LED abstraction — compiles to WS2812 (default) or onboard GP25 blink patterns (`USE_PICO_ONBOARD_LED`) |
| `modules/gpio/` | Flight Controller GPIO interface (ENABLE / RECORD input pins) |
| `modules/sdio/` | FatFS helpers — WAV file creation, config read/write |
| `config/` | `default.conf` parser, `fpv_sl_conf_t` definition |
| `usb/` | TinyUSB MSC + CDC descriptors and disk backend |
| `utils/` | `debug_log.h` (`LOGE/LOGW/LOGI/LOGD` macros), string cast helpers |
| `drivers/no-OS-FatFS-SD-SPI-RPi-Pico/` | Third-party FatFS over SPI SD library |

### Pin assignment

Default pins (overridable via `USE_CUSTOM_BOARD_PINS` in `modules/fpv_sl_core_board_pin.h`):

| Signal | GPIO |
|---|---|
| I2S SD/SCK/WS | 26 / 27 / 28 |
| SPI SD SCK/MOSI/MISO/CS | 10 / 11 / 12 / 13 |
| FC ENABLE / RECORD | 1 / 2 (production) — 2 / 3 (debug probe) |
| WS2812 LED | 16 |
| Onboard LED (debug) | 25 |

## On-target test runner

Firmware de test séparé (`fpv_sl_test_runner.uf2`) qui s'exécute sur le Pico et reporte via USB CDC.

```bash
# Configure avec les tests activés (debug probe recommandé)
cmake -DFPV_SL_PICO_PROBE_DEBUG=ON -DFPV_SL_BUILD_TARGET_TESTS=ON -S src -B src/build -G Ninja

# Build uniquement le test runner
cmake --build src/build --target fpv_sl_test_runner

# Binaire produit
src/build/tests/target/fpv_sl_test_runner.uf2
```

**Câblage requis** (mode debug probe) : `GP8 → GP2` (simulateur ENABLE) et `GP9 → GP3` (simulateur RECORD).

**Suites** (`src/tests/target/`) :
- `test_gpio_interface.c` — 6 tests IRQ GPIO via `gpio_sim_set_enable/record(bool)` (state-based, pas pulse)
- `test_recording_mode.c` — présent sur disque mais **non compilé** (logique pure, couvert par les tests host)

**USB** : le test runner utilise `pico_enable_stdio_usb 1` (CDC-ACM natif SDK, pas de MSC). Il ne faut **pas** lier `usb` ni `utils` — `debug_log_stub.c` fournit `debug_log_printf` via `printf`. Le logging fonctionne parce que `gpio` → `fpv_sl_logging` INTERFACE propage `DEBUG_LOG_ENABLE` + l'include `src/utils`.

Framework minimaliste dans `test_framework.h` (`TEST_EXPECT_*`, `RUN_TEST`). Résultat en LED : vert fixe = tout PASS, rouge clignotant = échec(s).

## Host tests (Unity)

Tests natifs compilés et exécutés sur PC, sans hardware. Depuis la racine du repo :

```bash
# Configure (première fois)
cmake -S src/tests -B src/tests/build_host

# Build
cmake --build src/tests/build_host

# Lancer tous les tests
ctest --test-dir src/tests/build_host -V
```

**Suites disponibles** (`src/tests/host/`) :

| Suite | Module testé | Dépendances Pico stubées |
|---|---|---|
| `test_cast_from_str` | `utils/cast_from_str.c` | aucune |
| `test_audio_buffer` | `modules/audio_buffer/` | `pico/critical_section.h` |
| `test_config_parser` | `parse_conf_key_value`, `string_to_key_enum` (dans `file_helper.c`) | `ff.h` (FatFS) |
| `test_dsp_filter` | `process_sample` (dans `fpv_sl_core.c`) | `pico/multicore.h`, `pico/mutex.h`, `ff.h` + link stubs |
| `test_recording_mode` | `get_mode_from_config`, `fpv_sl_process_mode` (dans `fpv_sl_core.c`) | mêmes que `test_dsp_filter` |

Les stubs sont dans `src/tests/stubs/` : headers inline (`pico/`, `ff.h`) et `pico_stubs.c` pour les symboles link-time.

**Note** : `process_sample` n'est pas déclarée dans `fpv_sl_core.h` (le header public déclare `apply_filter_and_gain`). La déclaration forward est dans `test_dsp_filter.c`.

### Logging

`DEBUG_LOG_ENABLE` and `LOG_LEVEL=LOG_LEVEL_DBG` are defined globally via the `fpv_sl_logging` interface library in `src/CMakeLists.txt`. Use `LOGI/LOGD/LOGW/LOGE` macros from `utils/debug_log.h`. Output goes over USB CDC.

---

## Prochaines étapes

Mettre à jour cette section à chaque fin de session. Cocher / supprimer une ligne quand c'est fait.

- [x] **Implémenter `fpv_sl_process_mode()`** — 3 boucles réelles implémentées (`ALWAY_RCD_TYPE`, `RCD_ONLY_TYPE`, `CLASSIC_TYPE`). `multicore_launch_core1` déplacé au début de `fpv_sl_process_mode`. `fpv_sl_core0_loop` gère la durée max via `g_max_record_ms`. `gpio_interface` étendu à 4 callbacks (RISE+FALL sur les deux pins FC). `update_disk_status()` câblé après chaque `finalize_wav_file()`.
- [x] **Câbler `update_disk_status()`** — appelé après chaque `finalize_wav_file()` dans les 3 modes.
- [x] **Migrer `test_recording_mode.c` en host test** — `src/tests/host/test_recording_mode.c`, 5 tests Unity, mêmes dépendances que `test_dsp_filter` (`fpv_sl_core.c` + `pico_stubs.c`).
- [x] **Vérifier / supprimer `read_config_file`** — déclaration supprimée de `file_helper.h`, `src/utils/cpcpy.txt` (brouillon de prototypage) supprimé.
- [ ] **Latence au montage MSC (TinyUSB)** — investiguer le délai observé lors de l'énumération / montage du volume SD en mode USB Mass Storage.
- [ ] **Vitesse de transfert en mode MSC** — mesurer et optimiser le débit de transfert des fichiers WAV via le mode MSC TinyUSB.
- [x] **Pré-allocation WAV via `f_expand()`** — implémenté avec fallback, checkpoint header dans `sync_wav_file()`, truncate dans `finalize_wav_file()` et `recover_unfinalized_recording()`. Clé `MAX_RECORD_DURATION` ajoutée (défaut 300 s). Voir section *WAV file lifecycle* dans Architecture.
- [x] **Flush audio sur triple-trigger ENABLE** (`DELETE_ON_TRIPLE_ARM`) — implémenté. Comptage dans `fpv_sl_on_enable()` (commun GPIO+MSP), `flush_audio_files()` dans `file_helper.c`, LED `set_module_flushing_status()` (rouge fixe prod, triple flash rapide debug). Check idle dans `RCD_ONLY` et `CLASSIC`. 4 tests Unity host (fenêtre, compteur, flag, clear). README documenté + note câblage ENABLE obligatoire en `RCD_ONLY`.
- [ ] **Filtre passe-bas + passe-bande** — ajouter un filtre passe-bas IIR symétrique au passe-haut existant (`process_sample`) pour obtenir un passe-bande configurable (ex. 200 Hz – 8 kHz). Fréquence de coupure haute à exposer dans `default.conf`.
- [ ] **Gain automatique (AGC)** — étudier un contrôle automatique de gain pour normaliser le niveau du signal selon l'amplitude mesurée sur fenêtre glissante.
- [x] **[MSP 1/5] Clé de config `use_uart_msp`** — `use_uart_msp` (bool), `msp_uart_id` (uint8), `msp_baud_rate` (uint32) dans `fpv_sl_conf_t` + parser + defaults (false / UART1 / 115200). Documenté dans README.
- [ ] **[MSP 2/5] Polling MSP ARM trigger** — init UART dédié, parser MSP minimal (`MSP_STATUS` cmd 101 uniquement). Machine d'état 2 phases : 100 Hz en idle (latence arm ≤ 10 ms) → 30 Hz en recording. Déclenche l'enregistrement I2S DMA en remplacement ou complément des GPIO ENABLE/RECORD. Synchro avec la vidéo DJI par le même événement arm FC.
- [ ] **[MSP 3/5] Struct télémétrie `fpv_sl_telemetry_t`** — struct binaire fixe timestampée (`uint32_t ms`) : RC channels 4 sticks + AUX (uint16 ×N), attitude roll/pitch/yaw (int16 ×3), GPS fix/lat/lon/alt/speed, analog tension/mAh/RSSI/ampères. Format binaire compact, fichier `.tlm` sur SD corrélable avec l'audio par timestamp.
- [ ] **[MSP 4/5] Valorisation MSP de la struct** — étendre le polling 30 Hz avec `MSP_RC` (105) + `MSP_ATTITUDE` (108) + `MSP_RAW_GPS` (106) + `MSP_ANALOG` (110) → remplissage `fpv_sl_telemetry_t` → écriture SD bufferisée pour ne pas bloquer Core 0. ~145 bytes/cycle × 30 Hz = ~4 kB/s.
- [ ] **[MSP 5/5] Support MAVLink (ArduPilot / avions)** — second parser sur le même UART (auto-détection ou clé `use_uart_mavlink`). `HEARTBEAT` → arm trigger (même machine d'état 2 phases). Messages périodiques push → mêmes champs `fpv_sl_telemetry_t`. Couvre iNAV en MAVLink et ArduPilot fixed-wing.
