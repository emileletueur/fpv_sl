add_library(msp msp_interface.c)
target_include_directories(msp PUBLIC ${CMAKE_CURRENT_LIST_DIR})
target_link_libraries(msp PUBLIC msp_driver fpv_sl_config fpv_sl_logging pico_stdlib hardware_timer)
