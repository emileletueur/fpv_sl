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

int main(void) {
    stdio_init_all();
    sleep_ms(1500);  /* laisser le temps à l'hôte de détecter le CDC */

    status_indicator_init();
    set_module_powered_status();

    /* ── Lancement des suites ──────────────────────────────────── */

    LOGI("═══════════════════════════════════════════════");
    LOGI("  fpv_sl — on-target test runner");
    LOGI("═══════════════════════════════════════════════");

    run_gpio_tests();

    /* ── Résultat final ────────────────────────────────────────── */

    LOGI("───────────────────────────────────────────────");
    if (_fail_count == 0) {
        LOGI("RESULT : %d / %d PASS  \u2713", _pass_count, _pass_count + _fail_count);
        set_module_record_ready_status();   /* LED verte fixe */
    } else {
        LOGE("RESULT : %d PASS  %d FAIL  \u2717", _pass_count, _fail_count);
        set_module_free_disk_critical_status();  /* LED rouge clignotante */
    }
    LOGI("═══════════════════════════════════════════════");

    while (1) {
        tight_loop_contents();
    }
}
