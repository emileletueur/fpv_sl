add_library(msp_driver msp_driver.c)
target_include_directories(msp_driver PUBLIC ${CMAKE_CURRENT_LIST_DIR})
target_link_libraries(msp_driver PUBLIC fpv_sl_logging pico_stdlib hardware_uart hardware_gpio)
