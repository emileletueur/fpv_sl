#include "unity.h"
#include "fpv_sl_core.h"
#include "fpv_sl_config.h"

extern uint32_t test_time_ms;

void setUp(void) {
    test_time_ms = 0;
    fpv_sl_reset_enable_pulse_counter();
    fpv_sl_clear_delete_request();
}
void tearDown(void) {}

/* conf_is_loaded = false → retourne erreur */
void test_conf_not_loaded_returns_error(void) {
    fpv_sl_conf_t conf = {.conf_is_loaded = false};
    TEST_ASSERT_NOT_EQUAL(0, (int)get_mode_from_config(&conf));
}

/* always_rcd = true → ALWAY_RCD_TYPE */
void test_mode_always_rcd(void) {
    fpv_sl_conf_t conf = {.conf_is_loaded = true, .always_rcd = true, .use_enable_pin = false};
    TEST_ASSERT_EQUAL(0, (int)get_mode_from_config(&conf));
}

/* always_rcd = false, use_enable_pin = false → RCD_ONLY_TYPE */
void test_mode_rcd_only(void) {
    fpv_sl_conf_t conf = {.conf_is_loaded = true, .always_rcd = false, .use_enable_pin = false};
    TEST_ASSERT_EQUAL(0, (int)get_mode_from_config(&conf));
}

/* always_rcd = false, use_enable_pin = true → CLASSIC_TYPE */
void test_mode_classic(void) {
    fpv_sl_conf_t conf = {.conf_is_loaded = true, .always_rcd = false, .use_enable_pin = true};
    TEST_ASSERT_EQUAL(0, (int)get_mode_from_config(&conf));
}

/* always_rcd prioritaire sur use_enable_pin */
void test_always_rcd_overrides_enable_pin(void) {
    fpv_sl_conf_t conf = {.conf_is_loaded = true, .always_rcd = true, .use_enable_pin = true};
    TEST_ASSERT_EQUAL(0, (int)get_mode_from_config(&conf));
}

/* ── Triple-trigger ENABLE ─────────────────────────────────────────────── */

/* 3 appels rapides → flag positionné */
void test_triple_trigger_sets_flag(void) {
    fpv_sl_on_enable();
    fpv_sl_on_enable();
    fpv_sl_on_enable();
    TEST_ASSERT_TRUE(fpv_sl_is_delete_requested());
}

/* 2 appels → flag non positionné */
void test_two_triggers_no_flag(void) {
    fpv_sl_on_enable();
    fpv_sl_on_enable();
    TEST_ASSERT_FALSE(fpv_sl_is_delete_requested());
}

/* 2 appels, fenêtre expirée, 1 appel → compteur remis à 1, pas de flag */
void test_window_expiry_resets_counter(void) {
    fpv_sl_on_enable(); /* pulse 1 à t=0 */
    fpv_sl_on_enable(); /* pulse 2 à t=0 */
    test_time_ms = 6000; /* avance au-delà de la fenêtre 5 s */
    fpv_sl_on_enable(); /* pulse 1 d'un nouveau comptage */
    TEST_ASSERT_FALSE(fpv_sl_is_delete_requested());
}

/* fpv_sl_clear_delete_request() efface bien le flag */
void test_clear_delete_request_works(void) {
    fpv_sl_on_enable();
    fpv_sl_on_enable();
    fpv_sl_on_enable();
    TEST_ASSERT_TRUE(fpv_sl_is_delete_requested());
    fpv_sl_clear_delete_request();
    TEST_ASSERT_FALSE(fpv_sl_is_delete_requested());
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_conf_not_loaded_returns_error);
    RUN_TEST(test_mode_always_rcd);
    RUN_TEST(test_mode_rcd_only);
    RUN_TEST(test_mode_classic);
    RUN_TEST(test_always_rcd_overrides_enable_pin);
    RUN_TEST(test_triple_trigger_sets_flag);
    RUN_TEST(test_two_triggers_no_flag);
    RUN_TEST(test_window_expiry_resets_counter);
    RUN_TEST(test_clear_delete_request_works);
    return UNITY_END();
}
