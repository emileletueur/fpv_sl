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
