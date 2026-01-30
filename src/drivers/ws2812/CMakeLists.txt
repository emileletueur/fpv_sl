add_library(ws2812
    ws2812.c
    ws2812.pio
)

pico_generate_pio_header(ws2812
    ${CMAKE_CURRENT_LIST_DIR}/ws2812.pio
)

target_include_directories(ws2812
    PUBLIC
    ${CMAKE_CURRENT_LIST_DIR})

target_link_libraries(ws2812
    fpv_sl_logging
    hardware_pio
    pico_stdlib
)
