#include "i2s_mic.h"
#include "debug_log.h"
#include "hardware/dma.h"
#include "hardware/irq.h"
#include "hardware/pio.h"
#include "i2s_mic.pio.h"
#include "pico/stdlib.h"
#include <hardware/clocks.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

static i2s_mic_t * i2s_mic = NULL;

// Structure du filtre Passe-Haut (High-Pass)
typedef struct {
    float alpha;
    float last_x;
    float last_y;
} hp_filter_t;

hp_filter_t filter_L = {0.959f, 0, 0}; // Alpha pour ~300Hz à 44.1kHz
hp_filter_t filter_R = {0.959f, 0, 0};

// --- FONCTION DE FILTRAGE ---
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

void dma_handler() {
    uint irq0_status = dma_hw->ints0;
    if (irq0_status & (1u << i2s_mic->dma_chan_ping)) {
        dma_hw->ints0 = (1u << i2s_mic->dma_chan_ping); // Clear l'interruption pour le canal A
        i2s_mic->buffer_to_process = 0;
    } else if (irq0_status & (1u << i2s_mic->dma_chan_pong)) {
        dma_hw->ints0 = (1u << i2s_mic->dma_chan_pong); // Clear l'interruption pour le canal B
        i2s_mic->buffer_to_process = 1;
    }
}

void i2s_mic_start(void) {
    LOGI("Start I2S NEMS MIC DMA Channel (ping before pong !).");
    dma_channel_start(i2s_mic->dma_chan_ping);
}

void stop_i2s_mic_rcd(void);

void init_i2s_mic(i2s_mic_t *i2s_mic_config) {
    i2s_mic = i2s_mic_config;
    // Initialize PIO
    PIO pio = pio0;
    uint sm = 0;
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
    dma_channel_configure(i2s_mic->dma_chan_ping, &c_a, i2s_mic->buffer_ping, &pio->rxf[sm], i2s_mic->buffer_size, false);

    // Config Chan B
    dma_channel_config c_b = dma_channel_get_default_config(i2s_mic->dma_chan_pong);
    channel_config_set_transfer_data_size(&c_b, DMA_SIZE_32);
    channel_config_set_read_increment(&c_b, false);
    channel_config_set_write_increment(&c_b, true);
    channel_config_set_dreq(&c_b, pio_get_dreq(pio, sm, false));
    channel_config_set_chain_to(&c_b, i2s_mic->dma_chan_ping);
    dma_channel_configure(i2s_mic->dma_chan_pong, &c_b, i2s_mic->buffer_pong, &pio->rxf[sm], i2s_mic->buffer_size, false);

    // dma_channel_start(i2s_mic->dma_channel_ping);
    LOGI("I2S NEMS MIC Initialization done.");
}
