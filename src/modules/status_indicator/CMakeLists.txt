add_library(status_indicator
    status_indicator.c
)

target_include_directories(status_indicator PUBLIC ${CMAKE_CURRENT_LIST_DIR})

target_link_libraries(status_indicator
    ws2812
    hardware_pio
    pico_stdlib
)