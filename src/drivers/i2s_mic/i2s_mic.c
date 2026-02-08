#include "i2s_mic.h"
#include "debug_log.h"
#include "hardware/dma.h"
#include "hardware/irq.h"
#include "hardware/pio.h"
#include "i2s_mic.pio.h"
#include <hardware/clocks.h>
#include <hardware/timer.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>

static i2s_mic_t *i2s_mic = NULL;
static bool is_i2s_initialized = false;
static bool is_i2s_started = false;
PIO pio = pio0;
uint sm = 0;

void dma_handler() {
    uint irq0_status = dma_hw->ints0;
    if (irq0_status & (1u << i2s_mic->dma_chan_ping)) {
        dma_hw->ints0 = (1u << i2s_mic->dma_chan_ping);
        i2s_mic->is_ping_buffer_ready = true;
        i2s_mic->active_buffer_ptr = i2s_mic->buffer_ping;
        i2s_mic->current_data_count = i2s_mic->buffer_size;
        i2s_mic->data_ready = true;
    }
    if (irq0_status & (1u << i2s_mic->dma_chan_pong)) {
        dma_hw->ints0 = (1u << i2s_mic->dma_chan_pong);
        i2s_mic->is_pong_buffer_ready = true;
        i2s_mic->active_buffer_ptr = i2s_mic->buffer_pong;
        i2s_mic->current_data_count = i2s_mic->buffer_size;
        i2s_mic->data_ready = true;
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
    dma_channel_start(i2s_mic->dma_chan_ping);
    is_i2s_started = true;
    LOGI("I2S started.");
    return 0;
}

int8_t i2s_mic_stop(void) {
    if (!is_i2s_started) {
        LOGW("I2S need to be started before being stopped.");
        return -1;
    }
    dma_channel_abort(i2s_mic->dma_chan_ping);
    dma_channel_abort(i2s_mic->dma_chan_pong);
    i2s_set_state(pio, sm, false);
    LOGI("I2S stopped.");
    is_i2s_started = false;
    is_i2s_initialized = false;
    return 0;
}

void init_i2s_mic(i2s_mic_t *i2s_mic_config) {
    i2s_mic = i2s_mic_config;

    i2s_mic->is_ping_buffer_ready = false;
    i2s_mic->is_pong_buffer_ready = false;
    i2s_mic->data_ready = false;
    i2s_mic->active_buffer_ptr = NULL;
    i2s_mic->current_data_count = 0;

    uint offset = pio_add_program(pio, &audio_i2s_program);
    float divider = (float) clock_get_hz(clk_sys) / (i2s_mic->sample_rate * 32 * 2 * 2);
    audio_i2s_program_init(pio, sm, offset, PIN_I2S_NEMS_MIC_SD, PIN_I2S_NEMS_MIC_SCK, divider);

    // Config DMA Ping-Pong
    i2s_mic->dma_chan_ping = dma_claim_unused_channel(true);
    i2s_mic->dma_chan_pong = dma_claim_unused_channel(true);

    // Config Chan A
    dma_channel_config c_a = dma_channel_get_default_config(i2s_mic->dma_chan_ping);
    channel_config_set_transfer_data_size(&c_a, DMA_SIZE_32);
    channel_config_set_read_increment(&c_a, false);
    channel_config_set_write_increment(&c_a, true);
    channel_config_set_dreq(&c_a, pio_get_dreq(pio, sm, false));
    channel_config_set_chain_to(&c_a, i2s_mic->dma_chan_pong);
    dma_channel_configure(i2s_mic->dma_chan_ping, &c_a, i2s_mic->buffer_ping, &pio->rxf[sm], i2s_mic->buffer_size,
                          false);

    // Config Chan B
    dma_channel_config c_b = dma_channel_get_default_config(i2s_mic->dma_chan_pong);
    channel_config_set_transfer_data_size(&c_b, DMA_SIZE_32);
    channel_config_set_read_increment(&c_b, false);
    channel_config_set_write_increment(&c_b, true);
    channel_config_set_dreq(&c_b, pio_get_dreq(pio, sm, false));
    channel_config_set_chain_to(&c_b, i2s_mic->dma_chan_ping);
    dma_channel_configure(i2s_mic->dma_chan_pong, &c_b, i2s_mic->buffer_pong, &pio->rxf[sm], i2s_mic->buffer_size,
                          false);

    dma_channel_set_irq0_enabled(i2s_mic->dma_chan_ping, true);
    dma_channel_set_irq0_enabled(i2s_mic->dma_chan_pong, true);

    irq_set_exclusive_handler(DMA_IRQ_0, dma_handler);
    irq_set_enabled(DMA_IRQ_0, true);

    is_i2s_initialized = true;

    LOGI("I2S Initialization done.");
}
