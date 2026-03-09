#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "audio_buffer.h"

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
    uint32_t          sample_rate;
    bool              is_mono;
    uint32_t          buffer_size;  /* doit être égal à AUDIO_BLOCK_SAMPLES */
    uint8_t           dma_chan;     /* rempli par init_i2s_mic() */
    audio_pipeline_t *pipeline;    /* fourni par l'appelant avant init */
} i2s_mic_t;

void   init_i2s_mic(i2s_mic_t *config);
int8_t i2s_mic_start(void);
int8_t i2s_mic_stop(void);
