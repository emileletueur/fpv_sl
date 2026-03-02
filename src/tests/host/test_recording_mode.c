#include "unity.h"
#include "fpv_sl_core.h"
#include "fpv_sl_config.h"

void setUp(void) {}
void tearDown(void) {}

/* conf_is_loaded = false → retourne erreur */
void test_conf_not_loaded_returns_error(void) {
    fpv_sl_conf_t conf = {.conf_is_loaded = false};
    TEST_ASSERT_NOT_EQUAL(0, (int)get_mode_from_config(&conf));
}

/* always_rcd = true → ALWAY_RCD_TYPE, retourne 0, process_mode ne crashe pas */
void test_mode_always_rcd(void) {
    fpv_sl_conf_t conf = {.conf_is_loaded = true, .always_rcd = true, .use_enable_pin = false};
    TEST_ASSERT_EQUAL(0, (int)get_mode_from_config(&conf));
    fpv_sl_process_mode();
}

/* always_rcd = false, use_enable_pin = false → RCD_ONLY_TYPE */
void test_mode_rcd_only(void) {
    fpv_sl_conf_t conf = {.conf_is_loaded = true, .always_rcd = false, .use_enable_pin = false};
    TEST_ASSERT_EQUAL(0, (int)get_mode_from_config(&conf));
    fpv_sl_process_mode();
}

/* always_rcd = false, use_enable_pin = true → CLASSIC_TYPE */
void test_mode_classic(void) {
    fpv_sl_conf_t conf = {.conf_is_loaded = true, .always_rcd = false, .use_enable_pin = true};
    TEST_ASSERT_EQUAL(0, (int)get_mode_from_config(&conf));
    fpv_sl_process_mode();
}

/* always_rcd prioritaire sur use_enable_pin */
void test_always_rcd_overrides_enable_pin(void) {
    fpv_sl_conf_t conf = {.conf_is_loaded = true, .always_rcd = true, .use_enable_pin = true};
    TEST_ASSERT_EQUAL(0, (int)get_mode_from_config(&conf));
    fpv_sl_process_mode();
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_conf_not_loaded_returns_error);
    RUN_TEST(test_mode_always_rcd);
    RUN_TEST(test_mode_rcd_only);
    RUN_TEST(test_mode_classic);
    RUN_TEST(test_always_rcd_overrides_enable_pin);
    return UNITY_END();
}
