#include "debug_cdc.h"
#include "diskio.h"
#include "ff.h"
#include "hw_config.h"

void msc_sd_init(void) {
    // Monter la carte SD
    FRESULT fr = f_mount(&fs, "0:", 1);
    if (fr == FR_OK) {
        sd_mounted = true;
        debug_cdc("SD card mounted successfully");
    } else {
        debug_cdc("SD mount failed: %d", fr);
    }
}

// Configuration SPI pour la carte SD
static spi_t spi_config = {
    .hw_inst = spi0, // ou spi1
    .miso_gpio = 16, // Adapter à vos pins
    .mosi_gpio = 19,
    .sck_gpio = 18,
    .baud_rate = 12500 * 1000 // 12.5 MHz
};

// Configuration de la carte SD
static sd_card_t sd_card = {.pcName = "0:",
                            .spi = &spi_config,
                            .ss_gpio = 17, // Chip Select
                            .use_card_detect = false,
                            .card_detect_gpio = -1,
                            .card_detected_true = -1};

size_t sd_get_num(void) {
    return 1;
}

sd_card_t *sd_get_by_num(size_t num) {
    if (num == 0) {
        return &sd_card;
    }
    return NULL;
}
