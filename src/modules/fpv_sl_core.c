

#include "fpv_sl_core.h"
#include "audio_buffer.h"
#include "pico/mutex.h"
#include "file_helper.h"
#include "i2s_mic.h"
#include <pico/multicore.h>
#include <stdint.h>

static const fpv_sl_conf_t *fpv_sl_conf = NULL;
hp_filter_t filter_L = {0.959f, 0, 0}; // Alpha pour ~300Hz à 44.1kHz
static execution_condition_t execution_condition;

// ─────────────────────────────────────────────
// Pipeline partagé entre les cores
// ─────────────────────────────────────────────

static audio_pipeline_t g_audio_pipeline;

// Flag d'arrêt propre
static volatile bool g_recording = false;



uint8_t get_mode_from_config(const fpv_sl_conf_t *fpv_sl_config) {
    fpv_sl_conf = fpv_sl_config;
    if (fpv_sl_conf->conf_is_loaded) {
        if (fpv_sl_conf->always_rcd)
            execution_condition = ALWAY_RCD_TYPE;
        else if (!fpv_sl_conf->always_rcd && !fpv_sl_conf->use_enable_pin)
            execution_condition = RCD_ONLY_TYPE;
        else if (!fpv_sl_conf->always_rcd && fpv_sl_conf->use_enable_pin)
            execution_condition = CLASSIC_TYPE;
        return 0;
    } else
        return -1;
}

void fpv_sl_process_mode(void) {
    switch (execution_condition) {
    case CLASSIC_TYPE:
        // Setup temporary file to write
        // waiting for ENABLE trigger to start I2S DMA
        // set LED indicator
        // waiting for ARM trigger to write sound data
        // set LED indicator
        // Record until DESARM
        // set LED indicator
        // Finalize with WAV header
        // Rename file with final computed name
        // Update file index in conf file
    case RCD_ONLY_TYPE:
        // Setup temporary file to write and start I2S DMA
        // waiting for ARM trigger to write sound data
        // Record until DESARM
        // Finalize with WAV header
        // Rename file with final computed name
        // Update file index in conf file
    case ALWAY_RCD_TYPE:
        // Setup temporary file to write
        // start I2S DMA
        // Record until DESARM
        // Finalize with WAV header
        // Rename file with final computed name
        // Update file index in conf file
        break;
    }
}

int32_t process_sample(hp_filter_t *f, int32_t sample) {
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

void fpv_sl_core0_loop(void) {
    // Start the core 1
    multicore_launch_core1(fpv_sl_core1_loop);

    // is buffer filled and ready
    if (is_data_ready()) {
        // Provide buffer address to core 1
        multicore_fifo_push_blocking((uint32_t) get_active_buffer_ptr());
        // wait for data compute finished
        uint32_t filtered_buffer = multicore_fifo_pop_blocking();
        // write the sound data to sd
        write_buffer(&filtered_buffer);
    }

    // if (buffer_to_process != -1) {
    //     int32_t *buf_addr = (buffer_to_process == 0) ? buffer_A : buffer_B;
    //     buffer_to_process = -1; // Reset flag

    //     // 1. Envoyer le buffer au coeur 1 pour filtrage
    //     multicore_fifo_push_blocking((uint32_t) buf_addr);

    //     // 2. Attendre que le coeur 1 ait fini le filtrage
    //     uint32_t filtered_buffer = multicore_fifo_pop_blocking();

    //     // 3. Écrire le buffer filtré sur la carte SD
    //     // C'est ici que ça peut prendre du temps (FatFs f_write)
    //     write_to_sd((int32_t *) filtered_buffer, BUFFER_SIZE);
    // }
}

void fpv_sl_core1_loop(void) {

    while (1) {
        uint32_t addr = multicore_fifo_pop_blocking();
        int32_t *samples = (int32_t *) addr;

        if (fpv_sl_conf->is_mono_rcd) {
            // COMPACTION : On ne garde que le canal GAUCHE (indices pairs)
            int j = 0;
            for (int i = 0; i < fpv_sl_conf->buffer_size; i += 2) {
                samples[j] = process_sample(&filter_L, samples[i]);
                j++;
            }
            // Ici, les BUFFER_SIZE/2 premiers slots du buffer sont remplis
        } else {
            // MODE STEREO : On traite les deux (ou on traite G et on laisse D à 0)
            for (int i = 0; i < fpv_sl_conf->buffer_size; i += 2) {
                samples[i] = process_sample(&filter_L, samples[i]);
                samples[i + 1] = 0; // On peut mettre le canal droit à 0 si micro mono
            }
        }

        multicore_fifo_push_blocking(addr);
    }

    // while (1) {
    //     // Attend l'adresse du buffer rempli par le DMA
    //     uint32_t addr = multicore_fifo_pop_blocking();
    //     int32_t *samples = (int32_t *)addr;

    //     if (MONO_MODE) {
    //         // COMPACTION : On ne garde que le canal GAUCHE (indices pairs)
    //         int j = 0;
    //         for (int i = 0; i < BUFFER_SIZE; i += 2) {
    //             samples[j] = process_sample(samples[i], &filter_L);
    //             j++;
    //         }
    //         // Ici, les BUFFER_SIZE/2 premiers slots du buffer sont remplis
    //     } else {
    //         // MODE STEREO : On traite les deux (ou on traite G et on laisse D à 0)
    //         for (int i = 0; i < BUFFER_SIZE; i += 2) {
    //             samples[i]   = process_sample(samples[i], &filter_L);
    //             samples[i+1] = 0; // On peut mettre le canal droit à 0 si micro mono
    //         }
    //     }

    //     // On renvoie l'adresse au Coeur 0 pour dire "C'est prêt pour la SD"
    //     multicore_fifo_push_blocking(addr);
    // }
}
