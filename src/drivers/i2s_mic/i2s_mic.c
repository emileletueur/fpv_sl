#include "i2s_mic.h"
#include "audio_buffer.h"
#include "debug_log.h"
#include "hardware/dma.h"
#include "hardware/irq.h"
#include "hardware/pio.h"
#include "i2s_mic.pio.h"
#include <hardware/clocks.h>
#include <hardware/timer.h>
#include <stdbool.h>
#include <stdint.h>

static i2s_mic_t *i2s_mic          = NULL;
static bool       is_i2s_initialized = false;
static bool       is_i2s_started     = false;
PIO  pio = pio0;
uint sm  = 0;

/* IRQ DMA_IRQ_0 : un seul canal, relancé à chaque complétion.
   IMPORTANT : après complétion, TRANS_COUNT vaut 0. Il faut le réinitialiser
   avant de déclencher, sinon le canal redémarre avec count=0 (0 mots transférés).
   Séquence : write_addr sans trigger, puis trans_count + trigger (alias al1). */
void dma_handler(void) {
    uint irq0_status = dma_hw->ints0;
    if (irq0_status & (1u << i2s_mic->dma_chan)) {
        dma_hw->ints0 = (1u << i2s_mic->dma_chan);
        audio_pipeline_dma_complete(i2s_mic->pipeline);
        int32_t *next_buf = audio_pipeline_get_dma_buffer(i2s_mic->pipeline);
        dma_channel_set_write_addr(i2s_mic->dma_chan, next_buf, false);
        dma_channel_set_trans_count(i2s_mic->dma_chan, AUDIO_BLOCK_SAMPLES, true);
    }
}

int8_t i2s_mic_start(void) {
    if (!is_i2s_initialized) {
        LOGE("I2S not initialized. Call init_i2s_mic() first.");
        return -1;
    }
    if (is_i2s_started) {
        LOGW("I2S already started.");
        return 0;
    }
    i2s_set_state(pio, sm, true);
    busy_wait_us(100);
    dma_channel_start(i2s_mic->dma_chan);
    is_i2s_started = true;
    LOGI("I2S started.");
    return 0;
}

int8_t i2s_mic_stop(void) {
    if (!is_i2s_started) {
        LOGW("I2S need to be started before being stopped.");
        return -1;
    }
    dma_channel_abort(i2s_mic->dma_chan);
    i2s_set_state(pio, sm, false);
    LOGI("I2S stopped.");
    is_i2s_started    = false;
    is_i2s_initialized = false;
    return 0;
}

void init_i2s_mic(i2s_mic_t *i2s_mic_config) {
    i2s_mic = i2s_mic_config;

    if (i2s_mic->buffer_size != AUDIO_BLOCK_SAMPLES) {
        LOGE("init_i2s_mic: buffer_size=%lu != AUDIO_BLOCK_SAMPLES=%u — forced to AUDIO_BLOCK_SAMPLES.",
             (unsigned long)i2s_mic->buffer_size, AUDIO_BLOCK_SAMPLES);
    }

    uint  offset  = pio_add_program(pio, &audio_i2s_program);
    float divider = (float)clock_get_hz(clk_sys) / (i2s_mic->sample_rate * 32 * 2 * 2);
    audio_i2s_program_init(pio, sm, offset, PIN_I2S_NEMS_MIC_SD, PIN_I2S_NEMS_MIC_SCK, divider);

    i2s_mic->dma_chan = dma_claim_unused_channel(true);

    /* Premier buffer fourni par le pipeline — adresse SRAM valide. */
    int32_t *first_buf = audio_pipeline_get_dma_buffer(i2s_mic->pipeline);

    dma_channel_config c = dma_channel_get_default_config(i2s_mic->dma_chan);
    channel_config_set_transfer_data_size(&c, DMA_SIZE_32);
    channel_config_set_read_increment(&c, false);
    channel_config_set_write_increment(&c, true);
    channel_config_set_dreq(&c, pio_get_dreq(pio, sm, false));
    /* Self-chain = no-op : le canal s'arrête après complétion, l'IRQ le relance. */
    channel_config_set_chain_to(&c, i2s_mic->dma_chan);
    dma_channel_configure(i2s_mic->dma_chan, &c, first_buf, &pio->rxf[sm], AUDIO_BLOCK_SAMPLES, false);

    dma_channel_set_irq0_enabled(i2s_mic->dma_chan, true);
    irq_set_exclusive_handler(DMA_IRQ_0, dma_handler);
    irq_set_enabled(DMA_IRQ_0, true);

    is_i2s_initialized = true;
    LOGI("I2S Initialization done.");
}
