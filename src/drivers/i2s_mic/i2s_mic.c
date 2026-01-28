#include "i2s_mic.h"
#include "hardware/dma.h"
#include "hardware/irq.h"
#include "hardware/pio.h"
#include "i2s_mic.pio.h"
#include "pico/stdlib.h"
#include <hardware/clocks.h>
#include <stdio.h>
#include <stdlib.h>

#define PIN_SD 2         // Data
#define PIN_SCK 3        // Clock (Base pour SCK et WS)


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

// --- HANDLER DMA ---
void dma_handler() {
    dma_hw->ints0 = 1u << 0; // Clear interrupt
    buffer_ready = true;     // Signale au main de traiter les données
}

void init_i2s_mic(i2s_mic_conf_t *i2s_config) {
    // Initialize PIO
    PIO pio = pio0;
    uint sm = 0;
    DMA_bufffer_t dma_buffer = create_buffer(i2s_config->buffer_size);
    uint offset = pio_add_program(pio, &audio_i2s_program);
    float freq_sck = i2s_config->sample_rate * 32 * i2s_config->is_mono_rcd;
    float divider = (float) clock_get_hz(clk_sys) / (freq_sck * 2);
    audio_i2s_program_init(pio, sm, offset, PIN_SD, PIN_SCK, divider);

    // Initialyze DMA
    i2s_config->dma_channel = dma_claim_unused_channel(true);
    dma_channel_config dma_channel_config = dma_channel_get_default_config(i2s_config->dma_channel);
    channel_config_set_read_increment(&dma_channel_config, false);
    channel_config_set_write_increment(&dma_channel_config, true);
    channel_config_set_dreq(&dma_channel_config, pio_get_dreq(pio, sm, false));
    channel_config_set_transfer_data_size(&dma_channel_config, DMA_SIZE_32);

    dma_channel_configure(i2s_config->dma_channel, &dma_channel_config, dma_buffer.data, &pio->rxf[sm],
                          i2s_config->buffer_size, false);
}

void i2s_loop(i2s_mic_conf_t *i2s_config) {

    // 3. Boucle principale
    while (true) {
        if (i2s_config->buffer_ready) {
            for (int i = 0; i < i2s_config->buffer_size; i++) {
                if (i % 2 == 0) {
                    i2s_config->dma_buffer.data[i] = apply_filter_and_gain(&filter_L, i2s_config->dma_buffer.data[i]);
                } else {
                    i2s_config->dma_buffer.data[i] = apply_filter_and_gain(&filter_R, i2s_config->dma_buffer.data[i]);
                }
            }
            i2s_config->buffer_ready = false;
            // Ici, vous pouvez envoyer dma_buffer vers USB ou SD
            printf("Traitement terminé sur un buffer\n");

            // Relancer le DMA pour le prochain tour
            dma_channel_set_write_addr(i2s_config->dma_channel, i2s_config->dma_buffer.data, true);
        }
    }
}
