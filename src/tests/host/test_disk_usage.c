#include "unity.h"
#include "fatfs_getfree_stub.h"
#include "file_helper.h"

void setUp(void)    {}
void tearDown(void) {}

/* ── Cas nominaux — calcul du pourcentage ────────────────────────────────── */

void test_empty_disk_is_0_percent(void) {
    stub_set_disk(1000, 1000, FR_OK);   /* tout libre */
    uint8_t pct = 0xFF;
    TEST_ASSERT_EQUAL_INT8(0, get_disk_usage_percent(&pct));
    TEST_ASSERT_EQUAL_UINT8(0, pct);
}

void test_half_full_is_50_percent(void) {
    stub_set_disk(1000, 500, FR_OK);
    uint8_t pct = 0xFF;
    TEST_ASSERT_EQUAL_INT8(0, get_disk_usage_percent(&pct));
    TEST_ASSERT_EQUAL_UINT8(50, pct);
}

void test_alert_threshold_is_80_percent(void) {
    stub_set_disk(1000, 200, FR_OK);    /* 800 / 1000 = 80% */
    uint8_t pct = 0xFF;
    TEST_ASSERT_EQUAL_INT8(0, get_disk_usage_percent(&pct));
    TEST_ASSERT_EQUAL_UINT8(80, pct);
}

void test_just_below_alert_is_79_percent(void) {
    stub_set_disk(1000, 210, FR_OK);    /* 790 / 1000 = 79% */
    uint8_t pct = 0xFF;
    TEST_ASSERT_EQUAL_INT8(0, get_disk_usage_percent(&pct));
    TEST_ASSERT_EQUAL_UINT8(79, pct);
}

void test_critical_threshold_is_95_percent(void) {
    stub_set_disk(1000, 50, FR_OK);     /* 950 / 1000 = 95% */
    uint8_t pct = 0xFF;
    TEST_ASSERT_EQUAL_INT8(0, get_disk_usage_percent(&pct));
    TEST_ASSERT_EQUAL_UINT8(95, pct);
}

void test_full_disk_is_100_percent(void) {
    stub_set_disk(1000, 0, FR_OK);
    uint8_t pct = 0xFF;
    TEST_ASSERT_EQUAL_INT8(0, get_disk_usage_percent(&pct));
    TEST_ASSERT_EQUAL_UINT8(100, pct);
}

/* ── Cas d'erreur ─────────────────────────────────────────────────────────── */

void test_f_getfree_failure_returns_error(void) {
    stub_set_disk(1000, 500, FR_DISK_ERR);
    uint8_t pct = 0xFF;
    TEST_ASSERT_EQUAL_INT8(-1, get_disk_usage_percent(&pct));
}

void test_total_clusters_zero_returns_error(void) {
    stub_set_disk(0, 0, FR_OK);         /* n_fatent = 2 → total_clust = 0 */
    uint8_t pct = 0xFF;
    TEST_ASSERT_EQUAL_INT8(-1, get_disk_usage_percent(&pct));
}

/* ── main ────────────────────────────────────────────────────────────────── */

int main(void) {
    UNITY_BEGIN();

    RUN_TEST(test_empty_disk_is_0_percent);
    RUN_TEST(test_half_full_is_50_percent);
    RUN_TEST(test_alert_threshold_is_80_percent);
    RUN_TEST(test_just_below_alert_is_79_percent);
    RUN_TEST(test_critical_threshold_is_95_percent);
    RUN_TEST(test_full_disk_is_100_percent);
    RUN_TEST(test_f_getfree_failure_returns_error);
    RUN_TEST(test_total_clusters_zero_returns_error);

    return UNITY_END();
}
