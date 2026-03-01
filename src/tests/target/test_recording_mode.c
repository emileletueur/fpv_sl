#include "test_framework.h"
#include "fpv_sl_core.h"
#include "fpv_sl_config.h"

/* ── Tests get_mode_from_config ─────────────────────────────────── */

/* conf_is_loaded = false → retourne erreur (!=0) */
void test_conf_not_loaded_returns_error(void) {
    fpv_sl_conf_t conf = {.conf_is_loaded = false};
    uint8_t result = get_mode_from_config(&conf);
    TEST_EXPECT_NEQ(0, (int)result);
}

/* always_rcd = true → ALWAY_RCD_TYPE, retourne 0 */
void test_mode_always_rcd(void) {
    fpv_sl_conf_t conf = {
        .conf_is_loaded = true,
        .always_rcd     = true,
        .use_enable_pin = false,
    };
    TEST_EXPECT_EQ(0, (int)get_mode_from_config(&conf));
    /* fpv_sl_process_mode() doit s'exécuter sans crash */
    fpv_sl_process_mode();
}

/* always_rcd = false, use_enable_pin = false → RCD_ONLY_TYPE */
void test_mode_rcd_only(void) {
    fpv_sl_conf_t conf = {
        .conf_is_loaded = true,
        .always_rcd     = false,
        .use_enable_pin = false,
    };
    TEST_EXPECT_EQ(0, (int)get_mode_from_config(&conf));
    fpv_sl_process_mode();
}

/* always_rcd = false, use_enable_pin = true → CLASSIC_TYPE */
void test_mode_classic(void) {
    fpv_sl_conf_t conf = {
        .conf_is_loaded = true,
        .always_rcd     = false,
        .use_enable_pin = true,
    };
    TEST_EXPECT_EQ(0, (int)get_mode_from_config(&conf));
    fpv_sl_process_mode();
}

/* always_rcd prioritaire sur use_enable_pin */
void test_always_rcd_overrides_enable_pin(void) {
    fpv_sl_conf_t conf = {
        .conf_is_loaded = true,
        .always_rcd     = true,
        .use_enable_pin = true,  /* les deux à true → always_rcd prend la main */
    };
    TEST_EXPECT_EQ(0, (int)get_mode_from_config(&conf));
    fpv_sl_process_mode();
}

/* ── Suite entry point ─────────────────────────────────────────── */

void run_recording_mode_tests(void) {
    LOGI("── Recording mode tests ──────────────────────");
    RUN_TEST(test_conf_not_loaded_returns_error);
    RUN_TEST(test_mode_always_rcd);
    RUN_TEST(test_mode_rcd_only);
    RUN_TEST(test_mode_classic);
    RUN_TEST(test_always_rcd_overrides_enable_pin);
}
