add_library(i2s_mic
    i2s_mic.c
    i2s_mic.pio
)

pico_generate_pio_header(i2s_mic
    ${CMAKE_CURRENT_LIST_DIR}/i2s_mic.pio
)

target_include_directories(i2s_mic PUBLIC ${CMAKE_CURRENT_LIST_DIR})

target_link_libraries(i2s_mic
    hardware_pio
    pico_stdlib
    hardware_pio
    hardware_dma
)
