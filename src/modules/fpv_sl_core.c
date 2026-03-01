

#include "fpv_sl_core.h"
#include "audio_buffer.h"
#include "pico/mutex.h"
#include "file_helper.h"
#include "i2s_mic.h"
#include "status_indicator.h"
#include "debug_log.h"
#include <pico/multicore.h>
#include <stdint.h>

static const fpv_sl_conf_t *fpv_sl_conf = NULL;
hp_filter_t filter_L = {0.959f, 0, 0}; // Alpha pour ~300Hz à 44.1kHz
static execution_condition_t execution_condition;

/* Nombre de blocs audio écrits entre deux f_sync().
   256 samples/bloc à 44180 Hz → 1 bloc ≈ 5.8 ms → 64 blocs ≈ 370 ms. */
#define SYNC_PERIOD_BLOCKS 64

// ─────────────────────────────────────────────
// Pipeline partagé entre les cores
// ─────────────────────────────────────────────

static audio_pipeline_t g_audio_pipeline;

// Flag d'arrêt propre
static volatile bool g_recording = false;



/* Lit l'espace disque et met à jour la LED en conséquence.
   À appeler après chaque finalize_wav_file(). */
static void update_disk_status(void) {
    uint8_t usage_pct;
    if (get_disk_usage_percent(&usage_pct) != 0) {
        LOGW("Disk usage check failed, skipping LED update.");
        return;
    }
    if (usage_pct >= 95) {
        LOGE("Disk critical: %d%% used.", usage_pct);
        set_module_free_disk_critical_status();
    } else if (usage_pct >= 80) {
        LOGW("Disk alert: %d%% used.", usage_pct);
        set_module_free_disk_alert_status();
    } else {
        LOGI("Disk OK: %d%% used.", usage_pct);
    }
}

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
        // finalize_wav_file(rcd_duration);
        // update_disk_status();   ← check espace après chaque fichier
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
    multicore_launch_core1(fpv_sl_core1_loop);

    uint32_t blocks_since_sync = 0;

    while (g_recording) {
        if (!is_data_ready()) {
            continue;
        }

        // Envoyer le bloc au Core 1 pour filtrage
        multicore_fifo_push_blocking((uint32_t)get_active_buffer_ptr());
        // Récupérer l'adresse du bloc filtré
        uint32_t filtered_addr = multicore_fifo_pop_blocking();
        // Écrire sur SD
        write_buffer((uint32_t *)filtered_addr);

        blocks_since_sync++;
        if (blocks_since_sync >= SYNC_PERIOD_BLOCKS) {
            sync_wav_file();
            blocks_since_sync = 0;
        }
    }
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
