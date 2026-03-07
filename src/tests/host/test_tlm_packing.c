#include "unity.h"
#include "tlm_writer.h"

void setUp(void) {}
void tearDown(void) {}

void test_file_header_size(void) {
    TEST_ASSERT_EQUAL_UINT(8u, sizeof(tlm_file_header_t));
}

void test_record_size_empty(void) {
    TEST_ASSERT_EQUAL_UINT(4u, tlm_record_size(0));
}

void test_record_size_rc(void) {
    TEST_ASSERT_EQUAL_UINT(20u, tlm_record_size(TLM_RC));
}

void test_record_size_attitude(void) {
    TEST_ASSERT_EQUAL_UINT(10u, tlm_record_size(TLM_ATTITUDE));
}

void test_record_size_gps(void) {
    TEST_ASSERT_EQUAL_UINT(18u, tlm_record_size(TLM_GPS));
}

void test_record_size_analog(void) {
    TEST_ASSERT_EQUAL_UINT(11u, tlm_record_size(TLM_ANALOG));
}

void test_record_size_all(void) {
    uint8_t all = TLM_RC | TLM_ATTITUDE | TLM_GPS | TLM_ANALOG;
    TEST_ASSERT_EQUAL_UINT(47u, tlm_record_size(all));
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_file_header_size);
    RUN_TEST(test_record_size_empty);
    RUN_TEST(test_record_size_rc);
    RUN_TEST(test_record_size_attitude);
    RUN_TEST(test_record_size_gps);
    RUN_TEST(test_record_size_analog);
    RUN_TEST(test_record_size_all);
    return UNITY_END();
}
