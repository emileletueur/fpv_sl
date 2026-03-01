add_executable(fpv_sl_test_runner
    test_runner_main.c
    test_gpio_interface.c
    debug_log_stub.c
)

pico_set_program_name(fpv_sl_test_runner "fpv_sl_test_runner")
pico_set_program_version(fpv_sl_test_runner "0.1")

# CDC natif du SDK — enumère comme CDC-ACM uniquement (pas de MSC)
pico_enable_stdio_uart(fpv_sl_test_runner 0)
pico_enable_stdio_usb(fpv_sl_test_runner 1)

target_include_directories(fpv_sl_test_runner PRIVATE
    ${CMAKE_CURRENT_LIST_DIR}     # test_framework.h
    ${CMAKE_SOURCE_DIR}           # src/ (pour les includes relatifs du projet)
    ${CMAKE_SOURCE_DIR}/modules   # gpio_interface.h via modules/
)

target_link_libraries(fpv_sl_test_runner
    pico_stdlib
    pico_unique_id
    gpio
    status_indicator
    ws2812
)

pico_add_extra_outputs(fpv_sl_test_runner)
