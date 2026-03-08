

#include "fpv_sl_core.h"
#include "debug_log.h"
#include "file_helper.h"
#include "i2s_mic.h"
#include "msp/msp_interface.h"
#include "pico/mutex.h"
#include "pico/time.h"
#include "status_indicator.h"
#include "telemetry/tlm_writer.h"
#include <math.h>
#include <pico/multicore.h>
#include <stdint.h>

#ifndef M_PI
#define M_PI 3.14159265f
#endif

static const fpv_sl_conf_t *fpv_sl_conf = NULL;

/* CDC Simulator : fpv_sl_cdc_task() est défini dans fpv_sl_loader.c quand
   FPV_SL_CDC_SIM est actif — appelé via cdc_poll() qui est un no-op sinon. */
#ifdef FPV_SL_CDC_SIM
extern void fpv_sl_cdc_task(void);
static inline void cdc_poll(void) { fpv_sl_cdc_task(); }
#else
static inline void cdc_poll(void) {}
#endif

static hp_filter_t filter_L    = {0};
static hp_filter_t filter_R    = {0};
static lp_filter_t filter_L_lp = {0};
static lp_filter_t filter_R_lp = {0};
static float       g_gain      = 0.8f; /* défaut = 80 % ; écrasé par get_mode_from_config() */
static execution_condition_t execution_condition;

/* Nombre de blocs audio écrits entre deux f_sync().
   256 samples/bloc à 44180 Hz → 1 bloc ≈ 5.8 ms → 64 blocs ≈ 370 ms. */
#define SYNC_PERIOD_BLOCKS 64

// ─────────────────────────────────────────────
// Pipeline partagé entre les cores
// ─────────────────────────────────────────────


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

float compute_hp_alpha(uint16_t cutoff_hz, uint16_t sample_rate) {
    float wc = 2.0f * M_PI * (float)cutoff_hz;
    float fs = (float)sample_rate;
    return fs / (fs + wc);
}

float compute_lp_alpha(uint16_t cutoff_hz, uint16_t sample_rate) {
    float wc = 2.0f * M_PI * (float)cutoff_hz;
    float fs = (float)sample_rate;
    return wc / (fs + wc);
}

uint8_t get_mode_from_config(const fpv_sl_conf_t *fpv_sl_config) {
    fpv_sl_conf = fpv_sl_config;
    if (!fpv_sl_conf->conf_is_loaded)
        return -1;

    /* Initialise les coefficients des filtres depuis la config. */
    filter_L.alpha    = compute_hp_alpha(fpv_sl_conf->high_pass_cutoff_freq, fpv_sl_conf->sample_rate);
    filter_R.alpha    = filter_L.alpha;
    filter_L_lp.alpha = compute_lp_alpha(fpv_sl_conf->low_pass_cutoff_freq,  fpv_sl_conf->sample_rate);
    filter_R_lp.alpha = filter_L_lp.alpha;
    g_gain = (float)fpv_sl_conf->mic_gain / 100.0f;
    LOGI("HP alpha=%.4f (fc=%uHz), LP alpha=%.4f (fc=%uHz) at %uHz, gain=%.2f (%u%%).",
         filter_L.alpha,    fpv_sl_conf->high_pass_cutoff_freq,
         filter_L_lp.alpha, fpv_sl_conf->low_pass_cutoff_freq,
         fpv_sl_conf->sample_rate, g_gain, fpv_sl_conf->mic_gain);

    if (fpv_sl_conf->record_on_boot)
        execution_condition = ALWAY_RCD_TYPE;
    else if (!fpv_sl_conf->use_enable_pin)
        execution_condition = RCD_ONLY_TYPE;
    else
        execution_condition = CLASSIC_TYPE;
    return 0;
}

void fpv_sl_process_mode(void) {
    multicore_launch_core1(fpv_sl_core1_loop);

    /* Démarrage I2S anticipé : DMA actif avant le trigger ARM,
       ring buffer déjà rempli → zéro latence au premier write SD. */
    if (execution_condition != CLASSIC_TYPE)
        i2s_mic_start();

    switch (execution_condition) {

    case ALWAY_RCD_TYPE:
        /* Enregistrement continu, découpé en fichiers de max_record_duration s. */
        g_max_record_ms = (uint32_t)fpv_sl_conf->max_record_duration * 1000UL;
        LOGI("ALWAY_RCD — max file duration %lu ms.", g_max_record_ms);

        /* Si MSP actif : attendre que la LiPo soit connectée (vbat >= lipo_min_mv).
           Permet de ne pas enregistrer quand le FC tourne sur USB en pré-vol. */
        if (fpv_sl_conf->use_uart_msp) {
            LOGI("ALWAY_RCD — attente LiPo MSP.");
            while (!msp_is_lipo_connected()) {
                msp_poll_if_due();
                if (g_delete_requested) {
                    LOGI("ALWAY_RCD — triple-trigger pendant attente LiPo: flush.");
                    set_module_flushing_status();
                    flush_audio_files();
                    g_delete_requested = false;
                    set_module_powered_status();
                }
                cdc_poll();
                tight_loop_contents();
            }
            LOGI("ALWAY_RCD — LiPo connectée, démarrage enregistrement.");
        }

        while (1) {
            create_wav_file();
            if (fpv_sl_conf->use_uart_msp && fpv_sl_conf->telemetry_items)
                tlm_writer_open(fpv_sl_conf->telemetry_items, TLM_PROTOCOL_MSP, 30u);
            set_module_recording_status();
            g_recording = true;
            uint32_t start_ms = to_ms_since_boot(get_absolute_time());
            fpv_sl_core0_loop();
            uint32_t duration_ms = to_ms_since_boot(get_absolute_time()) - start_ms;
            LOGI("ALWAY_RCD — file closed, duration %lu ms.", duration_ms);
            tlm_writer_close(fpv_sl_conf->file_index,
                             fpv_sl_conf->record_folder, fpv_sl_conf->record_prefix);
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
                msp_poll_if_due();
                if (g_delete_requested) {
                    LOGI("RCD_ONLY — triple-trigger: flush audio files.");
                    set_module_flushing_status();
                    flush_audio_files();
                    g_delete_requested = false;
                    create_wav_file();
                    set_module_record_ready_status();
                }
                cdc_poll();
                tight_loop_contents();
            }
            set_module_recording_status();
            LOGI("RCD_ONLY — ARM reçu, début enregistrement.");
            if (fpv_sl_conf->use_uart_msp && fpv_sl_conf->telemetry_items)
                tlm_writer_open(fpv_sl_conf->telemetry_items, TLM_PROTOCOL_MSP, 30u);
            uint32_t start_ms = to_ms_since_boot(get_absolute_time());
            fpv_sl_core0_loop();
            uint32_t duration_ms = to_ms_since_boot(get_absolute_time()) - start_ms;
            LOGI("RCD_ONLY — DISARM reçu, durée %lu ms.", duration_ms);
            tlm_writer_close(fpv_sl_conf->file_index,
                             fpv_sl_conf->record_folder, fpv_sl_conf->record_prefix);
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
            while (!g_enabled) {
                msp_poll_if_due();
                cdc_poll();
                tight_loop_contents();
            }

            if (g_delete_requested) {
                LOGI("CLASSIC — triple-trigger: flush audio files.");
                set_module_flushing_status();
                flush_audio_files();
                g_delete_requested = false;
                /* Attend que g_enabled retombe (l'utilisateur relâche le 3ème trigger). */
                while (g_enabled) {
                    msp_poll_if_due();
                    cdc_poll();
                    tight_loop_contents();
                }
                continue;
            }

            LOGI("CLASSIC — ENABLE reçu, démarrage I2S.");
            i2s_mic_start();
            create_wav_file();
            set_module_record_ready_status();

            while (g_enabled) {
                while (g_enabled && !g_recording) {
                    msp_poll_if_due();
                    cdc_poll();
                    tight_loop_contents();
                }
                if (!g_enabled)
                    break;
                set_module_recording_status();
                LOGI("CLASSIC — ARM reçu, début enregistrement.");
                if (fpv_sl_conf->use_uart_msp && fpv_sl_conf->telemetry_items)
                    tlm_writer_open(fpv_sl_conf->telemetry_items, TLM_PROTOCOL_MSP, 30u);
                uint32_t start_ms = to_ms_since_boot(get_absolute_time());
                fpv_sl_core0_loop();
                uint32_t duration_ms = to_ms_since_boot(get_absolute_time()) - start_ms;
                LOGI("CLASSIC — DISARM reçu, durée %lu ms.", duration_ms);
                tlm_writer_close(fpv_sl_conf->file_index,
                                 fpv_sl_conf->record_folder, fpv_sl_conf->record_prefix);
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

int32_t process_sample(hp_filter_t *hp, lp_filter_t *lp, int32_t sample) {
    /* 1. Alignement LSB : INMP441 envoie 24 bits dans un mot 32 bits MSB-aligné. */
    float x = (float)(sample >> 8);

    /* 2. Filtre passe-haut IIR 1er ordre (bypass si hp == NULL).
          y[n] = alpha * (y[n-1] + x[n] - x[n-1]) */
    float y;
    if (hp) {
        y = hp->alpha * (hp->last_y + x - hp->last_x);
        hp->last_x = x;
        hp->last_y = y;
    } else {
        y = x;
    }

    /* 3. Filtre passe-bas IIR 1er ordre (bypass si lp == NULL).
          y[n] = alpha * x[n] + (1 - alpha) * y[n-1] */
    if (lp) {
        float y_lp = lp->alpha * y + (1.0f - lp->alpha) * lp->last_y;
        lp->last_y = y_lp;
        y = y_lp;
    }

    /* 4. Gain configurable (MIC_GAIN %). */
    return (int32_t)(y * g_gain);
}

void fpv_sl_core0_loop(void) {
    filter_L.last_x = 0;    filter_L.last_y = 0;
    filter_R.last_x = 0;    filter_R.last_y = 0;
    filter_L_lp.last_y = 0; filter_R_lp.last_y = 0;

    uint32_t blocks_since_sync = 0;
    uint32_t start_ms          = to_ms_since_boot(get_absolute_time());

    while (g_recording) {
        if (msp_poll_if_due() && fpv_sl_conf->telemetry_items) {
            uint8_t rec[TLM_RECORD_MAX];
            uint8_t rlen = msp_get_telemetry_record(fpv_sl_conf->telemetry_items, rec);
            if (rlen > 0)
                tlm_writer_write(rec, rlen);
        }
        if (!is_data_ready()) {
            cdc_poll();
            tight_loop_contents();
            continue;
        }

        multicore_fifo_push_blocking((uint32_t) get_active_buffer_ptr());
        uint32_t filtered_addr = multicore_fifo_pop_blocking();
        __dmb();
        write_buffer((uint32_t *) filtered_addr);
        cdc_poll();

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

    uint32_t overruns = i2s_mic_get_overrun_count();
    if (overruns > 0)
        LOGW("I2S: %lu bloc(s) perdus (overrun DMA — Core 0 trop lent).", overruns);
    else
        LOGI("I2S: aucun overrun.");
}

void fpv_sl_core1_loop(void) {

    while (1) {
        uint32_t addr = multicore_fifo_pop_blocking();
        int32_t *samples = (int32_t *) addr;

        hp_filter_t *hp_l = fpv_sl_conf->use_high_pass_filter ? &filter_L    : NULL;
        hp_filter_t *hp_r = fpv_sl_conf->use_high_pass_filter ? &filter_R    : NULL;
        lp_filter_t *lp_l = fpv_sl_conf->use_low_pass_filter  ? &filter_L_lp : NULL;
        lp_filter_t *lp_r = fpv_sl_conf->use_low_pass_filter  ? &filter_R_lp : NULL;

        if (fpv_sl_conf->mono_record) {
            /* COMPACTION : canal GAUCHE (indices pairs) uniquement. */
            int j = 0;
            for (int i = 0; i < fpv_sl_conf->buffer_size; i += 2) {
                samples[j++] = process_sample(hp_l, lp_l, samples[i]);
            }
        } else {
            /* STEREO : canal gauche (L/R=GND) + canal droit (L/R=VCC). */
            for (int i = 0; i < fpv_sl_conf->buffer_size; i += 2) {
                samples[i]     = process_sample(hp_l, lp_l, samples[i]);
                samples[i + 1] = process_sample(hp_r, lp_r, samples[i + 1]);
            }
        }

        __dmb();
        multicore_fifo_push_blocking(addr);
    }
}
