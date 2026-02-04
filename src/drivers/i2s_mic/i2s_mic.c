#include "i2s_mic.h"
#include "debug_log.h"
#include "hardware/dma.h"
#include "hardware/irq.h"
#include "hardware/pio.h"
#include "i2s_mic.pio.h"
#include "pico/stdlib.h"
#include <hardware/clocks.h>
#include <stdio.h>
#include <stdlib.h>

static int i2s_dma_channel;
static volatile bool is_dma_transfer_done = false;

int dma_chan_a, dma_chan_b;

DMA_bufffer_t create_buffer(size_t size) {
    DMA_bufffer_t buf;
    buf.data = (int32_t *) malloc(size * sizeof(int32_t));
    buf.size = size;
    if (buf.data == NULL) {
        perror("malloc failed");
        exit(EXIT_FAILURE);
    }
    for (size_t i = 0; i < buf.size; i++)
        buf.data[i] = 0;
    return buf;
}

void free_buffer(DMA_bufffer_t *buf) {
    free(buf->data);
    buf->data = NULL;
    buf->size = 0;
}

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
    dma_hw->ints0 = 1u << 0;
    is_dma_transfer_done = true;
}

void i2s_mic_start(void) {
    LOGI("Start I2S NEMS MIC DMA Channel.");
    is_dma_transfer_done = false;
    dma_channel_start(i2s_dma_channel);
}

void stop_i2s_mic_rcd(void);

void init_i2s_mic(i2s_mic_t *i2s_mic) {

    // Initialize PIO
    PIO pio = pio0;
    uint sm = 0;
    uint offset = pio_add_program(pio, &audio_i2s_program);
    float divider = (float) clock_get_hz(clk_sys) / (i2s_mic->sample_rate * 32 * 2 * 2);
    audio_i2s_program_init(pio, sm, offset, PIN_I2S_NEMS_MIC_SD, PIN_I2S_NEMS_MIC_SCK, divider);

    // Config DMA Ping-Pong
    dma_chan_a = dma_claim_unused_channel(true);
    dma_chan_b = dma_claim_unused_channel(true);

    // Config Chan A
    dma_channel_config c_a = dma_channel_get_default_config(dma_chan_a);
    channel_config_set_transfer_data_size(&c_a, DMA_SIZE_32);
    channel_config_set_read_increment(&c_a, false);
    channel_config_set_write_increment(&c_a, true);
    channel_config_set_dreq(&c_a, pio_get_dreq(pio, sm, false));
    channel_config_set_chain_to(&c_a, dma_chan_b);
    dma_channel_configure(dma_chan_a, &c_a, i2s_mic->buffer_ping, &pio->rxf[sm], i2s_mic->buffer_size, false);

    // Config Chan B
    dma_channel_config c_b = dma_channel_get_default_config(dma_chan_b);
    channel_config_set_transfer_data_size(&c_b, DMA_SIZE_32);
    channel_config_set_read_increment(&c_b, false);
    channel_config_set_write_increment(&c_b, true);
    channel_config_set_dreq(&c_b, pio_get_dreq(pio, sm, false));
    channel_config_set_chain_to(&c_b, dma_chan_a);
    dma_channel_configure(dma_chan_b, &c_b, i2s_mic->buffer_pong, &pio->rxf[sm], i2s_mic->buffer_size, false);

    dma_channel_start(dma_chan_a);
    LOGI("I2S NEMS MIC Initialization done.");
}
