#include "bsp/board_api.h"
#include "tusb.h"
#include "pico/stdlib.h"
#include "debug_log.h"
#include "status_indicator.h"
#include "test_framework.h"

/* ── Variables globales du framework ───────────────────────────── */

int _test_failed = 0;
int _pass_count  = 0;
int _fail_count  = 0;

/* ── Déclarations des suites ────────────────────────────────────── */

void run_gpio_tests(void);
void run_recording_mode_tests(void);

/* ── Constantes ─────────────────────────────────────────────────── */

#define CDC_READY_TIMEOUT_MS 8000
#define CDC_SETTLE_MS         500

int main(void) {
    /* Init TinyUSB */
    tusb_rhport_init_t dev_init = {.role = TUSB_ROLE_DEVICE, .speed = TUSB_SPEED_AUTO};
    board_init_after_tusb();
    tusb_init(BOARD_TUD_RHPORT, &dev_init);

    /* LED : alimenté */
    status_indicator_init();
    set_module_powered_status();

    /* Attendre la connexion CDC (hôte USB) */
    uint32_t start = to_ms_since_boot(get_absolute_time());
    while (!tud_cdc_connected()) {
        tud_task();
        if (to_ms_since_boot(get_absolute_time()) - start > CDC_READY_TIMEOUT_MS)
            break;
    }
    sleep_ms(CDC_SETTLE_MS);

    /* ── Lancement des suites ──────────────────────────────────── */

    LOGI("═══════════════════════════════════════════════");
    LOGI("  fpv_sl — on-target test runner");
    LOGI("═══════════════════════════════════════════════");

    run_gpio_tests();
    run_recording_mode_tests();

    /* ── Résultat final ────────────────────────────────────────── */

    LOGI("───────────────────────────────────────────────");
    if (_fail_count == 0) {
        LOGI("RESULT : %d / %d PASS  ✓", _pass_count, _pass_count + _fail_count);
        set_module_record_ready_status();   /* LED verte fixe */
    } else {
        LOGE("RESULT : %d PASS  %d FAIL  ✗", _pass_count, _fail_count);
        set_module_free_disk_critical_status();  /* LED rouge clignotante */
    }
    LOGI("═══════════════════════════════════════════════");

    /* Maintenir le CDC actif pour que les logs restent lisibles */
    while (1) {
        tud_task();
    }
}
