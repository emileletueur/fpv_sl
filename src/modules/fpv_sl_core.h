#pragma once
// #define USE_CUSTOM_BOARD_PINS

#include "fpv_sl_config.h"

typedef enum {
    CLASSIC_TYPE, // use ENABLE && RECORD trigger, dma will shutdown if ENABLE = false
    RCD_ONLY_TYPE, // use RECORD only to trigger record, dma never sleep
    ALWAY_RCD_TYPE, // as soon as board is powered the recording is started
} execution_condition_t;

uint8_t compute_execution_condition(fpv_sl_conf_t *fpv_sl_conf);
void fpv_sl_core_loop(void);
