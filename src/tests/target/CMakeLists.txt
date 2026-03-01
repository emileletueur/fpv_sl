add_executable(fpv_sl_test_runner
    test_runner_main.c
    test_gpio_interface.c
    test_recording_mode.c
)

pico_set_program_name(fpv_sl_test_runner "fpv_sl_test_runner")
pico_set_program_version(fpv_sl_test_runner "0.1")

# Pas de stdio Pico standard — on passe par le CDC TinyUSB comme le firmware principal
pico_enable_stdio_uart(fpv_sl_test_runner 0)
pico_enable_stdio_usb(fpv_sl_test_runner 0)

target_include_directories(fpv_sl_test_runner PRIVATE
    ${CMAKE_CURRENT_LIST_DIR}     # test_framework.h
    ${CMAKE_SOURCE_DIR}           # src/ (pour les includes relatifs du projet)
    ${CMAKE_SOURCE_DIR}/modules   # fpv_sl_core.h, fpv_sl_core_board_pin.h
    ${CMAKE_SOURCE_DIR}/utils     # debug_log.h
)

target_link_libraries(fpv_sl_test_runner
    fpv_sl_config
    fpv_sl_core
    usb
    pico_stdlib
    pico_unique_id
    utils
    gpio
    sdio
    audio_buffer
    ws2812
    i2s_mic
    status_indicator
)

pico_add_extra_outputs(fpv_sl_test_runner)
