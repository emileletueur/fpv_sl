add_library(audio_buffer
    audio_buffer.c
)

target_include_directories(audio_buffer
    PUBLIC
    ${CMAKE_CURRENT_LIST_DIR}
)

target_link_libraries(audio_buffer PUBLIC
    fpv_sl_config
    pico_stdlib
    pico_sync
)
