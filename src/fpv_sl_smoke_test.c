/* fpv_sl_smoke_test.c
 *
 * Binaire de validation matérielle autonome : attend SMOKE_WAIT_MS,
 * enregistre SMOKE_RECORD_MS de son, finalise le fichier WAV, halt.
 *
 * Court-circuite : USB/TinyUSB, GPIO, MSP, machine d'état modes.
 * Valide        : SD mount, WAV lifecycle, I2S DMA, pipeline dual-core,
 *                 filtre HP, LED.
 *
 * Build :
 *   cmake -DFPV_SL_BUILD_SMOKE_TEST=ON \
 *         [-DSMOKE_WAIT_MS=10000] [-DSMOKE_RECORD_MS=10000] \
 *         -S src -B src/build -G Ninja
 *   cmake --build src/build --target fpv_sl_smoke_test
 */

#include "debug_log.h"
#include "file_helper.h"
#include "fpv_sl_core.h"
#include "i2s_mic.h"
#include "status_indicator.h"

#include "pico/multicore.h"
#include "pico/stdlib.h"
#include "pico/time.h"

#ifndef SMOKE_WAIT_MS
#define SMOKE_WAIT_MS 10000u
#endif

#ifndef SMOKE_RECORD_MS
#define SMOKE_RECORD_MS 10000u
#endif

/* Alarme tirée par le hardware timer après SMOKE_RECORD_MS —
   positionne g_recording=false via le callback standard fpv_sl_core. */
static int64_t stop_recording_cb(alarm_id_t id, void *user_data) {
    (void)id;
    (void)user_data;
    fpv_sl_on_disarm();
    return 0; /* ne pas réarmer */
}

static void halt_error(void) {
    LOGE("Smoke test: ERREUR — arrêt.");
    set_module_free_disk_critical_status();
    while (1)
        tight_loop_contents();
}

int main(void) {
    stdio_init_all();
    status_indicator_init();
    set_module_powered_status();

    LOGI("=== fpv_sl smoke test ===");
    LOGI("Attente %u ms avant enregistrement.", (unsigned)SMOKE_WAIT_MS);
    sleep_ms(SMOKE_WAIT_MS);

    /* ── SD card + config ───────────────────────────────────────────── */
    LOGI("Lecture config SD...");
    if (read_conf_file() != 0) {
        LOGE("Mount SD / lecture config échoué.");
        halt_error();
    }
    const fpv_sl_conf_t *conf = get_conf();
    LOGI("Config OK — %u Hz %s.", conf->sample_rate,
         conf->mono_record ? "mono" : "stereo");

    /* ── Init fpv_sl_core (positionne fpv_sl_conf interne + filtre) ── */
    get_mode_from_config(conf);

    /* ── Init I2S mic ───────────────────────────────────────────────── */
    i2s_mic_t mic_conf = {
        .sample_rate = conf->sample_rate,
        .is_mono     = conf->mono_record,
        .buffer_size = 2048,
    };
    init_i2s_mic(&mic_conf);
    LOGI("I2S init OK.");

    /* ── Création du fichier WAV ────────────────────────────────────── */
    LOGI("Création t_mic_rcd.wav...");
    create_wav_file();

    /* ── Lancement Core 1 (filtre HP) ──────────────────────────────── */
    multicore_launch_core1(fpv_sl_core1_loop);

    /* ── Démarrage DMA I2S ──────────────────────────────────────────── */
    i2s_mic_start();

    /* ── Alarme d'arrêt + début enregistrement ──────────────────────── */
    LOGI("Enregistrement %u ms...", (unsigned)SMOKE_RECORD_MS);
    set_module_recording_status();
    add_alarm_in_ms(SMOKE_RECORD_MS, stop_recording_cb, NULL, false);

    uint32_t start_ms = to_ms_since_boot(get_absolute_time());
    fpv_sl_on_record();
    fpv_sl_core0_loop(); /* bloque jusqu'à ce que l'alarme appelle fpv_sl_on_disarm() */
    uint32_t duration_ms = to_ms_since_boot(get_absolute_time()) - start_ms;

    /* ── Finalisation ───────────────────────────────────────────────── */
    i2s_mic_stop();
    LOGI("Finalisation WAV (durée réelle %lu ms)...", (unsigned long)duration_ms);
    finalize_wav_file(duration_ms);

    LOGI("=== Smoke test OK — fichier écrit. ===");
    set_module_record_ready_status();

    while (1)
        tight_loop_contents();
}
