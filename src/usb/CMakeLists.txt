add_library(usb STATIC
    usb_descriptor.c
    msc_disk.c
    cdc/debug_cdc.c
)

target_include_directories(usb PUBLIC
    ${CMAKE_CURRENT_LIST_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/cdc
)

target_link_libraries(usb PUBLIC
    pico_stdlib
    tinyusb_device
    tinyusb_board
    hardware_pio
    FatFs_SPI
)
