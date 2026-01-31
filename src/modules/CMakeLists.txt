add_library(fpv_sl_core
    fpv_sl_core.c
)

target_include_directories(fpv_sl_core PUBLIC
    ${CMAKE_CURRENT_LIST_DIR}
)

target_link_libraries(fpv_sl_core PUBLIC
    fpv_sl_config
)
