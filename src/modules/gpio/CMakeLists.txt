add_library(gpio
    file_helper.c
)

target_include_directories(gpio
    PUBLIC
    ${CMAKE_CURRENT_LIST_DIR}
)

target_link_libraries(gpio
    PUBLIC
    pico_stdlib
)
