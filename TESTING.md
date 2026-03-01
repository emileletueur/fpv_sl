# Testing

Two levels of tests coexist in this project:

| Level | Where it runs | What it covers |
|---|---|---|
| **Host tests** | PC (native x86) | Pure logic — config parser, audio buffer state machine, DSP filter, string casts |
| **On-target tests** | Raspberry Pi Pico | Hardware-dependent — GPIO IRQ via FC simulators, recording mode dispatch |

---

## 1. Host tests

### Prerequisites

- CMake ≥ 3.13
- A native C compiler (`gcc` or `clang`)
- Internet access on first run (FetchContent downloads Unity v2.6.0 from GitHub)

### Setup

```bash
# Configure (once)
cmake -S src/tests -B src/tests/build_host

# Build
cmake --build src/tests/build_host
```

### Run

```bash
# All suites
ctest --test-dir src/tests/build_host -V

# Single suite
./src/tests/build_host/test_audio_buffer
./src/tests/build_host/test_config_parser
./src/tests/build_host/test_cast_from_str
./src/tests/build_host/test_dsp_filter
```

### Reading the output

Each line shows the test name and its result:

```
src/tests/host/test_audio_buffer.c:42:test_init_all_blocks_free:PASS
src/tests/host/test_audio_buffer.c:43:test_init_indices_zero:PASS
...
-----------------------
26 Tests 0 Failures 0 Ignored
OK
```

A failing test prints the assertion that did not hold and its file/line:

```
src/tests/host/test_audio_buffer.c:42:test_init_all_blocks_free:FAIL
Expected 0 Was 1
```

### Test suites

| Suite | Module under test | Key scenarios |
|---|---|---|
| `test_cast_from_str` | `utils/cast_from_str.c` | `parse_bool` (true/false/1/yes/invalid), `parse_uint8/16/32/64` boundaries and overflow |
| `test_audio_buffer` | `modules/audio_buffer/` | Full DMA→process→write→free cycle, overrun detection, ring index wrap, pending count |
| `test_config_parser` | `parse_conf_key_value`, `string_to_key_enum` (in `file_helper.c`) | Valid pairs, missing separator, newline/CRLF stripping, all key enums, case sensitivity |
| `test_dsp_filter` | `process_sample` (in `fpv_sl_core.c`) | DC rejection, first-sample gain, sign symmetry, Nyquist pass-through, alpha=0 |

### Stub strategy

The Pico SDK is not available on the host. Files in `src/tests/stubs/` replace the hardware headers:

| Stub | Replaces | Technique |
|---|---|---|
| `stubs/pico/critical_section.h` | `pico/critical_section.h` | `static inline` no-ops |
| `stubs/pico/mutex.h` | `pico/mutex.h` | `static inline` no-ops |
| `stubs/pico/multicore.h` | `pico/multicore.h` | Declarations only (implemented in `pico_stubs.c`) |
| `stubs/ff.h` | FatFS `ff.h` | `static inline` no-ops; `f_gets` returns `NULL` to stop read loops immediately |
| `stubs/pico_stubs.c` | Link-time symbols | `multicore_*`, `is_data_ready`, `get_active_buffer_ptr`, `write_buffer` |

### Adding a host test

1. Create `src/tests/host/test_<module>.c` following the pattern of an existing suite:
   - `void setUp(void) {}` / `void tearDown(void) {}`
   - One `void test_<scenario>(void)` function per scenario
   - A `main()` that calls `UNITY_BEGIN()`, `RUN_TEST(...)` for each function, `UNITY_END()`

2. Add a target in `src/tests/CMakeLists.txt`:
   ```cmake
   add_executable(test_<module>
       host/test_<module>.c
       ${SRC}/path/to/module.c   # only the .c files actually needed
   )
   target_include_directories(test_<module> PRIVATE
       ${SRC}/path/to/include
       ${STUBS}
       ${unity_SOURCE_DIR}/src
   )
   target_link_libraries(test_<module> PRIVATE unity)
   add_test(NAME <module> COMMAND test_<module>)
   ```

3. Rebuild and run: `cmake --build src/tests/build_host && ctest --test-dir src/tests/build_host -V`

---

## 2. On-target tests

The on-target test runner is a **separate firmware** (`fpv_sl_test_runner.uf2`). It replaces the main firmware on the Pico for the duration of the test session.

### Prerequisites

Same toolchain as the main firmware (see [Build](README.md#build)), plus a **debug probe** (Picoprobe or similar) and a serial terminal.

### Wiring — FC simulator

The GPIO simulator outputs must be wired to the FC input pins:

```
Pico GP8 ────────► Pico GP2   (ENABLE simulator → ENABLE input)
Pico GP9 ────────► Pico GP3   (RECORD simulator  → RECORD input)
```

> In production mode the FC inputs are on GP1/GP2. In debug probe mode
> (`FPV_SL_PICO_PROBE_DEBUG=ON`) they are remapped to GP2/GP3 because GP1/GP2
> are reserved for SWD (SWDIO/SWDCLK).

### Build

```bash
# Clean previous build if the option was not set before
rm -rf src/build

# Configure with both debug and test options
cmake -DFPV_SL_PICO_PROBE_DEBUG=ON -DFPV_SL_BUILD_TARGET_TESTS=ON \
      -S src -B src/build -G Ninja

# Build only the test runner
cmake --build src/build --target fpv_sl_test_runner
```

Output: `src/build/tests/target/fpv_sl_test_runner.uf2`

### Flash

Hold BOOTSEL, connect the Pico, release BOOTSEL. Copy the UF2:

```bash
cp src/build/tests/target/fpv_sl_test_runner.uf2 /media/$USER/RPI-RP2/
```

### Read results

Open the CDC serial port in a terminal (115200 baud, any serial monitor):

```bash
# Linux
screen /dev/ttyACM0 115200

# Or with minicom
minicom -D /dev/ttyACM0 -b 115200
```

Expected output:

```
═══════════════════════════════════════════════
  fpv_sl — on-target test runner
═══════════════════════════════════════════════
── GPIO interface tests ──────────────────────
[PASS] test_enable_callback_called
[PASS] test_record_callback_called
[PASS] test_no_retrigger_when_already_high
[PASS] test_second_rising_edge_triggers_again
[PASS] test_sequence_enable_then_record
[PASS] test_null_callbacks_no_crash
── Recording mode tests ──────────────────────
[PASS] test_conf_not_loaded_returns_error
[PASS] test_mode_always_rcd
[PASS] test_mode_rcd_only
[PASS] test_mode_classic
[PASS] test_always_rcd_overrides_enable_pin
───────────────────────────────────────────────
RESULT : 11 / 11 PASS  ✓
═══════════════════════════════════════════════
```

The **status LED** also reflects the final result:
- **Green fixed** — all tests passed
- **Red blinking** — one or more failures

A failing test prints the assertion and its location:

```
[ERR]     EXPECT failed: enable_cb_called  [test_gpio_interface.c:55]
[ERR] [FAIL] test_enable_callback_called
```

### Test suites

| Suite | File | Key scenarios |
|---|---|---|
| GPIO interface | `test_gpio_interface.c` | Rising edge triggers callback, no retrigger on stable HIGH, sequence ENABLE→RECORD, NULL callbacks |
| Recording mode | `test_recording_mode.c` | Config not loaded returns error, ALWAYS/RCD_ONLY/CLASSIC mode selection, `always_rcd` priority |

### Adding an on-target test

1. Add a test function in the relevant suite file (or create a new one):
   ```c
   void test_my_scenario(void) {
       // arrange
       gpio_sim_set_enable(false);
       sleep_us(50);
       // act
       gpio_sim_set_enable(true);
       sleep_us(200);
       // assert
       TEST_EXPECT_TRUE(my_flag);
   }
   ```

2. Register it in the suite's `run_*_tests()` function:
   ```c
   RUN_TEST(test_my_scenario);
   ```

3. If adding a new suite file, declare `void run_my_tests(void);` in `test_runner_main.c` and call it.

### Available assertions (`test_framework.h`)

| Macro | Checks |
|---|---|
| `TEST_EXPECT(cond)` | `cond` is true |
| `TEST_EXPECT_TRUE(v)` | `v != 0` |
| `TEST_EXPECT_FALSE(v)` | `v == 0` |
| `TEST_EXPECT_EQ(a, b)` | `a == b` |
| `TEST_EXPECT_NEQ(a, b)` | `a != b` |
| `TEST_EXPECT_NULL(v)` | `v == NULL` |
| `TEST_EXPECT_NOT_NULL(v)` | `v != NULL` |
