add_library(gpio
    gpio_interface.c
)

target_include_directories(gpio
    PUBLIC
    ${CMAKE_CURRENT_LIST_DIR}
)

target_link_libraries(gpio
    PUBLIC
    fpv_sl_logging
    pico_stdlib
)
