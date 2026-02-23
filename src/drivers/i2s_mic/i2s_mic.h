#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifndef PIN_I2S_NEMS_MIC_SD
#define PIN_I2S_NEMS_MIC_SD 26
#endif

#ifndef PIN_I2S_NEMS_MIC_SCK
#define PIN_I2S_NEMS_MIC_SCK 27
#endif

#ifndef PIN_I2S_NEMS_MIC_WS
#define PIN_I2S_NEMS_MIC_WS 28
#endif

typedef struct {
    uint32_t sample_rate;
    bool is_mono;
    uint32_t buffer_size;
    int32_t *buffer_ping;
    int32_t *buffer_pong;
    uint8_t dma_chan_ping;
    uint8_t dma_chan_pong;
    volatile bool data_ready;
    volatile int32_t *active_buffer_ptr;
    volatile uint32_t current_data_count;
} i2s_mic_t;

bool is_data_ready(void);
volatile int32_t* get_active_buffer_ptr(void);
uint32_t get_current_data_count(void);
void init_i2s_mic(i2s_mic_t *config);
int8_t i2s_mic_start(void);
int8_t i2s_mic_stop(void);
