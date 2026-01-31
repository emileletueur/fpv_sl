#pragma once
#ifdef USE_CUSTOM_BOARD_PINS

// SPI-SD PIN
#ifdef USE_SD_SPI
#define PIN_SD_SPI_SCK 10
#define PIN_SD_SPI_MOSI 11
#define PIN_SD_SPI_MISO 12
#define PIN_SD_SPI_CS 13
#endif

// I2S NEMS MIC PIN
#ifdef USE_I2S_NEMS_MIC
#define PIN_I2S_NEMS_MIC_SD 26
#define PIN_I2S_NEMS_MIC_SCK 27
#define PIN_I2S_NEMS_MIC_WS 28
#endif

// FC INTERFACE INPUT PIN
#ifdef USE_FC
#define PIN_FC_ENABLE_PIN 1
#define PIN_FC_RECORD_PIN 2
#endif

#endif
