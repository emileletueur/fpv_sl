#pragma once
// #define USE_CUSTOM_BOARD_PINS

#include "fpv_sl_config.h"

typedef enum {
    CLASSIC_TYPE, // use ENABLE && RECORD trigger, dma will shutdown if ENABLE = false
    RCD_ONLY_TYPE, // use RECORD only to trigger record, dma never sleep
    ALWAY_RCD_TYPE, // as soon as board is powered the recording is started
} execution_condition_t;

typedef struct {
    float alpha;
    float last_x;
    float last_y;
} hp_filter_t;

hp_filter_t filter_L = {0.959f, 0, 0}; // Alpha pour ~300Hz à 44.1kHz

int32_t apply_filter_and_gain(hp_filter_t *f, int32_t sample) {
    // 1. Décalage pour alignement LSB (comme discuté)
    int32_t x = sample >> 8;

    // 2. Filtre Passe-Haut
    float x_f = (float) x;
    float y_f = f->alpha * (f->last_y + x_f - f->last_x);
    f->last_x = x_f;
    f->last_y = y_f;

    // 3. Réduction de gain de 20% (on garde 80%)
    return (int32_t) (y_f * 0.8f);
}

uint8_t compute_execution_condition(fpv_sl_conf_t *fpv_sl_conf);
void fpv_sl_core0_loop(void);
