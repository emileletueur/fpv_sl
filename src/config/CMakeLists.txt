add_library(fpv_sl_config  INTERFACE)

target_include_directories(fpv_sl_config  INTERFACE
    ${CMAKE_CURRENT_LIST_DIR}
)

target_compile_definitions(fpv_sl_config INTERFACE
    USE_CUSTOM_BOARD_PINS
    USE_SD_SPI
)
