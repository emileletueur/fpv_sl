

#include "fpv_sl_core.h"
#include "audio_buffer.h"
#include "debug_log.h"
#include "file_helper.h"
#include "i2s_mic.h"
#include "pico/mutex.h"
#include "pico/time.h"
#include "status_indicator.h"
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

/* Flags positionnés par les callbacks GPIO (ou MSP à terme).
   Lus dans fpv_sl_process_mode() — écriture depuis IRQ uniquement. */
static volatile bool g_enabled   = false; /* ENABLE pin reçu (CLASSIC_TYPE) */
static volatile bool g_recording = false; /* ARM/RECORD pin reçu → écriture SD active */

/* Durée max pour ALWAY_RCD_TYPE (ms). 0 = pas de limite (RCD_ONLY / CLASSIC). */
static uint32_t g_max_record_ms = 0;

/* Triple-trigger ENABLE → demande de suppression des fichiers audio.
   Le compteur se réinitialise si la fenêtre ENABLE_PULSE_WINDOW_MS est dépassée.
   La logique est dans fpv_sl_on_enable() — valide pour GPIO et MSP. */
#define ENABLE_PULSE_WINDOW_MS 5000U
#define ENABLE_PULSE_COUNT     3U

static uint8_t       g_enable_pulse_count    = 0;
static uint32_t      g_enable_first_pulse_ms = 0;
static volatile bool g_delete_requested      = false;

bool fpv_sl_is_delete_requested(void)       { return g_delete_requested; }
void fpv_sl_clear_delete_request(void)      { g_delete_requested = false; }
void fpv_sl_reset_enable_pulse_counter(void){ g_enable_pulse_count = 0; }

int8_t fpv_sl_on_enable(void) {
    uint32_t now = to_ms_since_boot(get_absolute_time());

    if (g_enable_pulse_count == 0) {
        g_enable_first_pulse_ms = now;
        g_enable_pulse_count    = 1;
    } else if (now - g_enable_first_pulse_ms <= ENABLE_PULSE_WINDOW_MS) {
        g_enable_pulse_count++;
        if (g_enable_pulse_count >= ENABLE_PULSE_COUNT) {
            LOGI("Triple-trigger ENABLE: delete requested.");
            g_delete_requested   = true;
            g_enable_pulse_count = 0;
        }
    } else {
        /* Fenêtre expirée → repart d'un nouveau comptage. */
        g_enable_first_pulse_ms = now;
        g_enable_pulse_count    = 1;
    }

    g_enabled = true;
    return 0;
}

int8_t fpv_sl_on_disable(void) {
    g_enabled   = false;
    g_recording = false; /* stoppe l'enregistrement si actif */
    return 0;
}
int8_t fpv_sl_on_record(void)  { g_recording = true;  return 0; }
int8_t fpv_sl_on_disarm(void)  { g_recording = false; return 0; }

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
    multicore_launch_core1(fpv_sl_core1_loop);

    /* Démarrage I2S anticipé : DMA actif avant le trigger ARM,
       ring buffer déjà rempli → zéro latence au premier write SD. */
    if (execution_condition != CLASSIC_TYPE)
        i2s_mic_start();

    switch (execution_condition) {

    case ALWAY_RCD_TYPE:
        /* Enregistrement continu, découpé en fichiers de max_rcd_duration s. */
        g_max_record_ms = (uint32_t)fpv_sl_conf->max_rcd_duration * 1000UL;
        LOGI("ALWAY_RCD — max file duration %lu ms.", g_max_record_ms);
        while (1) {
            create_wav_file();
            set_module_recording_status();
            g_recording = true;
            uint32_t start_ms = to_ms_since_boot(get_absolute_time());
            fpv_sl_core0_loop();
            uint32_t duration_ms = to_ms_since_boot(get_absolute_time()) - start_ms;
            LOGI("ALWAY_RCD — file closed, duration %lu ms.", duration_ms);
            finalize_wav_file(duration_ms);
            update_disk_status();
        }
        break;

    case RCD_ONLY_TYPE:
        /* I2S déjà démarré. Pré-crée le fichier pour réduire la latence au ARM. */
        LOGI("RCD_ONLY — attente ARM.");
        create_wav_file();
        set_module_record_ready_status();
        while (1) {
            while (!g_recording) {
                if (g_delete_requested) {
                    LOGI("RCD_ONLY — triple-trigger: flush audio files.");
                    set_module_flushing_status();
                    flush_audio_files();
                    g_delete_requested = false;
                    create_wav_file();
                    set_module_record_ready_status();
                }
                tight_loop_contents();
            }
            set_module_recording_status();
            LOGI("RCD_ONLY — ARM reçu, début enregistrement.");
            uint32_t start_ms = to_ms_since_boot(get_absolute_time());
            fpv_sl_core0_loop();
            uint32_t duration_ms = to_ms_since_boot(get_absolute_time()) - start_ms;
            LOGI("RCD_ONLY — DISARM reçu, durée %lu ms.", duration_ms);
            finalize_wav_file(duration_ms);
            update_disk_status();
            create_wav_file();
            set_module_record_ready_status();
        }
        break;

    case CLASSIC_TYPE:
        /* I2S démarré à ENABLE uniquement. Boucle ENABLE → ARM/DISARM → DISABLE. */
        LOGI("CLASSIC — attente ENABLE.");
        while (1) {
            while (!g_enabled)
                tight_loop_contents();

            if (g_delete_requested) {
                LOGI("CLASSIC — triple-trigger: flush audio files.");
                set_module_flushing_status();
                flush_audio_files();
                g_delete_requested = false;
                /* Attend que g_enabled retombe (l'utilisateur relâche le 3ème trigger). */
                while (g_enabled)
                    tight_loop_contents();
                continue;
            }

            LOGI("CLASSIC — ENABLE reçu, démarrage I2S.");
            i2s_mic_start();
            create_wav_file();
            set_module_record_ready_status();

            while (g_enabled) {
                while (g_enabled && !g_recording)
                    tight_loop_contents();
                if (!g_enabled)
                    break;
                set_module_recording_status();
                LOGI("CLASSIC — ARM reçu, début enregistrement.");
                uint32_t start_ms = to_ms_since_boot(get_absolute_time());
                fpv_sl_core0_loop();
                uint32_t duration_ms = to_ms_since_boot(get_absolute_time()) - start_ms;
                LOGI("CLASSIC — DISARM reçu, durée %lu ms.", duration_ms);
                finalize_wav_file(duration_ms);
                update_disk_status();
                if (g_enabled) {
                    create_wav_file();
                    set_module_record_ready_status();
                }
            }

            LOGI("CLASSIC — DISABLE reçu, arrêt I2S.");
            i2s_mic_stop();
        }
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
    uint32_t blocks_since_sync = 0;
    uint32_t start_ms          = to_ms_since_boot(get_absolute_time());

    while (g_recording) {
        if (!is_data_ready()) {
            tight_loop_contents();
            continue;
        }

        multicore_fifo_push_blocking((uint32_t) get_active_buffer_ptr());
        uint32_t filtered_addr = multicore_fifo_pop_blocking();
        write_buffer((uint32_t *) filtered_addr);

        blocks_since_sync++;
        if (blocks_since_sync >= SYNC_PERIOD_BLOCKS) {
            sync_wav_file();
            blocks_since_sync = 0;
        }

        if (g_max_record_ms > 0) {
            uint32_t elapsed = to_ms_since_boot(get_absolute_time()) - start_ms;
            if (elapsed >= g_max_record_ms)
                g_recording = false;
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
}
