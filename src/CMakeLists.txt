# Generated Cmake Pico project file

cmake_minimum_required(VERSION 3.13)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Initialise pico_sdk from installed location
# (note this can come from environment, CMake cache etc)

# == DO NOT EDIT THE FOLLOWING LINES for the Raspberry Pi Pico VS Code Extension to work ==
if(WIN32)
    set(USERHOME $ENV{USERPROFILE})
else()
    set(USERHOME $ENV{HOME})
endif()
set(sdkVersion 2.2.0)
set(toolchainVersion 14_2_Rel1)
set(picotoolVersion 2.2.0-a4)
set(picoVscode ${USERHOME}/.pico-sdk/cmake/pico-vscode.cmake)
if (EXISTS ${picoVscode})
    include(${picoVscode})
endif()
# ====================================================================================
set(PICO_BOARD pico CACHE STRING "Board type")

# Pull in Raspberry Pi Pico SDK (must be before project)
include(pico_sdk_import.cmake)

project(fpv_sl_loader C CXX ASM)

# Initialise the Raspberry Pi Pico SDK
pico_sdk_init()

# Add subdirectories with additional CMakeLists.txt
add_subdirectory(drivers/no-OS-FatFS-SD-SPI-RPi-Pico/FatFs_SPI build)
add_subdirectory(modules/sdio)
add_subdirectory(utils)
add_subdirectory(drivers/ws2812)
add_subdirectory(drivers/i2s_mic)
add_subdirectory(modules/status_indicator)
add_subdirectory(usb)

# Add executable. Default name is the project name, version 0.1
add_executable(fpv_sl_loader
    fpv_sl_loader.c
)

pico_set_program_name(fpv_sl_loader "fpv_sl_loader")
pico_set_program_version(fpv_sl_loader "0.1")

# Modify the below lines to enable/disable output over UART/USB
pico_enable_stdio_uart(fpv_sl_loader 0)
pico_enable_stdio_usb(fpv_sl_loader 0)

# Add the standard include files to the build
target_include_directories(fpv_sl_loader PRIVATE
        ${CMAKE_CURRENT_LIST_DIR}
)

# Add the standard library to the build
target_link_libraries(fpv_sl_loader
        usb
        pico_stdlib
        pico_unique_id
        utils
        sdio
        ws2812
        i2s_mic
        status_indicator
)

pico_add_extra_outputs(fpv_sl_loader)
