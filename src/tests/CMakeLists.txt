cmake_minimum_required(VERSION 3.13)
project(fpv_sl_host_tests C)

set(CMAKE_C_STANDARD 11)

enable_testing()

# ── Unity (framework de test) ──────────────────────────────────────────────
include(FetchContent)
FetchContent_Declare(
    unity
    GIT_REPOSITORY https://github.com/ThrowTheSwitch/Unity.git
    GIT_TAG        v2.6.0
    GIT_SHALLOW    TRUE
)
FetchContent_MakeAvailable(unity)

# Raccourcis de chemin
set(SRC   ${CMAKE_CURRENT_SOURCE_DIR}/..)     # src/
set(STUBS ${CMAKE_CURRENT_SOURCE_DIR}/stubs)  # src/tests/stubs/

# ── test_cast_from_str ────────────────────────────────────────────────────
# Logique pure : aucun stub nécessaire.
add_executable(test_cast_from_str
    host/test_cast_from_str.c
    ${SRC}/utils/cast_from_str.c
)
target_include_directories(test_cast_from_str PRIVATE
    ${SRC}/utils
    ${unity_SOURCE_DIR}/src
)
target_link_libraries(test_cast_from_str PRIVATE unity)
add_test(NAME cast_from_str COMMAND test_cast_from_str)

# ── test_audio_buffer ─────────────────────────────────────────────────────
# Seule dépendance Pico : pico/critical_section.h → stub header suffisant.
add_executable(test_audio_buffer
    host/test_audio_buffer.c
    ${SRC}/modules/audio_buffer/audio_buffer.c
)
target_include_directories(test_audio_buffer PRIVATE
    ${SRC}/modules/audio_buffer
    ${STUBS}                         # pico/critical_section.h stub
    ${unity_SOURCE_DIR}/src
)
target_link_libraries(test_audio_buffer PRIVATE unity)
add_test(NAME audio_buffer COMMAND test_audio_buffer)

# ── test_config_parser ────────────────────────────────────────────────────
# file_helper.c contient parse_conf_key_value et string_to_key_enum.
# Les fonctions FatFS sont stubées via ff.h inline ; les macros de log
# sont des no-ops (DEBUG_LOG_ENABLE non défini).
add_executable(test_config_parser
    host/test_config_parser.c
    ${SRC}/modules/sdio/file_helper.c
    ${SRC}/utils/cast_from_str.c
)
target_include_directories(test_config_parser PRIVATE
    ${SRC}                           # pour les includes relatifs de file_helper.c
    ${SRC}/modules/sdio
    ${SRC}/utils
    ${SRC}/config
    ${STUBS}                         # ff.h stub
    ${unity_SOURCE_DIR}/src
)
target_link_libraries(test_config_parser PRIVATE unity)
add_test(NAME config_parser COMMAND test_config_parser)

# ── test_dsp_filter ───────────────────────────────────────────────────────
# fpv_sl_core.c contient process_sample.
# pico_stubs.c fournit les implémentations link-time des symboles Pico/HW.
add_executable(test_dsp_filter
    host/test_dsp_filter.c
    ${SRC}/modules/fpv_sl_core.c
    stubs/pico_stubs.c
)
target_include_directories(test_dsp_filter PRIVATE
    ${SRC}                           # inclus par fpv_sl_core.h → fpv_sl_config.h
    ${SRC}/modules
    ${SRC}/modules/audio_buffer      # audio_buffer.h (type audio_pipeline_t)
    ${SRC}/modules/sdio              # file_helper.h
    ${SRC}/drivers/i2s_mic           # i2s_mic.h
    ${SRC}/utils                     # debug_log.h (macros no-op sans DEBUG_LOG_ENABLE)
    ${SRC}/config
    ${STUBS}                         # pico/critical_section.h, pico/mutex.h,
                                     # pico/multicore.h, ff.h
    ${unity_SOURCE_DIR}/src
)
target_link_libraries(test_dsp_filter PRIVATE unity)
add_test(NAME dsp_filter COMMAND test_dsp_filter)
