#pragma once
// #define USE_CUSTOM_BOARD_PINS

#include "fpv_sl_config.h"

typedef enum {
    CLASSIC_TYPE,   // use ENABLE && RECORD trigger, dma will shutdown if ENABLE = false
    RCD_ONLY_TYPE,  // use RECORD only to trigger record, dma never sleep
    ALWAY_RCD_TYPE, // as soon as board is powered the recording is started
} execution_condition_t;

typedef struct {
    float alpha;
    float last_x;
    float last_y;
} hp_filter_t;

uint8_t get_mode_from_config(const fpv_sl_conf_t *fpv_sl_conf);
void fpv_sl_process_mode(void);
void fpv_sl_core0_loop(void);
void fpv_sl_core1_loop(void);
int32_t apply_filter_and_gain(hp_filter_t *f, int32_t sample);

/* Callbacks GPIO — à passer à initialize_gpio_interface().
   Positionnent les flags internes de fpv_sl_core ; ne font aucun accès SD ni I2S. */
int8_t fpv_sl_on_enable(void);
int8_t fpv_sl_on_record(void);
