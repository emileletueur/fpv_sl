add_library(telemetry tlm_writer.c)
target_include_directories(telemetry PUBLIC ${CMAKE_CURRENT_LIST_DIR})
target_link_libraries(telemetry PUBLIC fpv_sl_config fpv_sl_logging FatFs_SPI)
