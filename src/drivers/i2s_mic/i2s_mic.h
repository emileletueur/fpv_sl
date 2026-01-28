#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef struct {
    int32_t *data;
    size_t size;
} DMA_bufffer_t;

typedef struct {
    uint32_t sample_rate; // sample rate
    uint8_t is_mono_rcd;  // 1-> mono; 2-> stereo
    int32_t buffer_size;  // 1024
    int dma_channel;
    DMA_bufffer_t dma_buffer;
    bool buffer_ready;
} i2s_mic_conf_t;

void init_i2s_mic(i2s_mic_conf_t *i2s_config);
void start_i2s_mic_rcd(void);
void stop_i2s_mic_rcd(void);
