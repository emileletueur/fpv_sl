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

# ── Debug option: Pi Pico classique avec LED onboard (probe debugging) ───────
# Active USE_PICO_ONBOARD_LED dans status_indicator pour utiliser la LED GP25
# au lieu du RGB WS2812. Désactivé par défaut (build production).
#
# Usage: cmake -DFPV_SL_PICO_PROBE_DEBUG=ON ...
option(FPV_SL_PICO_PROBE_DEBUG "Debug avec Pi Pico standard (LED onboard GP25 au lieu de WS2812)" OFF)
if(FPV_SL_PICO_PROBE_DEBUG)
    message(STATUS "FPV_SL: mode debug probe — LED onboard (USE_PICO_ONBOARD_LED)")
    add_compile_definitions(USE_PICO_ONBOARD_LED)
endif()

add_library(fpv_sl_logging INTERFACE)
target_compile_definitions(fpv_sl_logging INTERFACE
    DEBUG_LOG_ENABLE
    LOG_LEVEL=LOG_LEVEL_DBG
)

target_include_directories(fpv_sl_logging INTERFACE
    ${CMAKE_CURRENT_LIST_DIR}/utils
)

# Add subdirectories with additional CMakeLists.txt
add_subdirectory(drivers/no-OS-FatFS-SD-SPI-RPi-Pico/FatFs_SPI build)
add_subdirectory(modules)
add_subdirectory(modules/gpio)
add_subdirectory(modules/sdio)
add_subdirectory(modules/audio_buffer)
add_subdirectory(utils)
add_subdirectory(config)
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
        ${CMAKE_CURRENT_LIST_DIR}/modules
        ${CMAKE_CURRENT_LIST_DIR}/utils
)

# Add the standard library to the build
target_link_libraries(fpv_sl_loader
        fpv_sl_config
        fpv_sl_core
        usb
        pico_stdlib
        pico_unique_id
        utils
        gpio
        sdio
        audio_buffer
        ws2812
        i2s_mic
        status_indicator
)

pico_add_extra_outputs(fpv_sl_loader)

# ── On-target test runner (firmware séparé) ───────────────────────────────
# Usage: cmake -DFPV_SL_BUILD_TARGET_TESTS=ON -S src -B src/build -G Ninja
option(FPV_SL_BUILD_TARGET_TESTS "Build on-target test runner firmware (.uf2)" OFF)
if(FPV_SL_BUILD_TARGET_TESTS)
    message(STATUS "FPV_SL: on-target tests activés — cible fpv_sl_test_runner")
    add_subdirectory(tests/target)
endif()
