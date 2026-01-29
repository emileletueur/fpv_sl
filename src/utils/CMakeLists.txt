add_library(utils
    cast_from_str.c
)

target_include_directories(utils
    PUBLIC
    ${CMAKE_CURRENT_LIST_DIR}
)

target_link_libraries(utils
    PUBLIC
    pico_stdlib
)
