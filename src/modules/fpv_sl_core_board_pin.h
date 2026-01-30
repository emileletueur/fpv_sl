#pragma once
#ifdef USE_CUSTOM_BOARD_PINS

// SPI-SD PIN
#ifdef USE_SD_SPI
#define PIN_SD_SPI_MISO 16
#define PIN_SD_SPI_MOSI 19
#define PIN_SD_SPI_SCK 18
#define PIN_SD_SPI_CS 17
#endif

// I2S NEMS MIC PIN
#ifdef USE_I2S_NEMS_MIC
#define PIN_I2S_NEMS_MIC_WS 16
#define PIN_I2S_NEMS_MIC_WS 19
#define PIN_I2S_NEMS_MIC_WS 18
#endif

// FC INTERFACE INPUT PIN
#ifdef USE_FC
#define PIN_FC_ENABLE_PIN 16
#define PIN_FC_RECORD_PIN 19
#endif

#endif
