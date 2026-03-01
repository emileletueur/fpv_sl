#include "unity.h"
#include "cast_from_str.h"

void setUp(void)    {}
void tearDown(void) {}

/* ── parse_bool ─────────────────────────────────────────────────────────── */

void test_parse_bool_true_literal(void) { TEST_ASSERT_TRUE(parse_bool("true")); }
void test_parse_bool_TRUE_upper(void)   { TEST_ASSERT_TRUE(parse_bool("TRUE")); }
void test_parse_bool_one(void)          { TEST_ASSERT_TRUE(parse_bool("1")); }
void test_parse_bool_yes(void)          { TEST_ASSERT_TRUE(parse_bool("yes")); }
void test_parse_bool_YES_upper(void)    { TEST_ASSERT_TRUE(parse_bool("YES")); }

void test_parse_bool_false_literal(void) { TEST_ASSERT_FALSE(parse_bool("false")); }
void test_parse_bool_zero(void)          { TEST_ASSERT_FALSE(parse_bool("0")); }
void test_parse_bool_empty(void)         { TEST_ASSERT_FALSE(parse_bool("")); }
void test_parse_bool_invalid(void)       { TEST_ASSERT_FALSE(parse_bool("xyz")); }
void test_parse_bool_no(void)            { TEST_ASSERT_FALSE(parse_bool("no")); }

/* ── parse_uint8 ─────────────────────────────────────────────────────────── */

void test_parse_uint8_nominal(void)  { TEST_ASSERT_EQUAL_UINT8(42,  parse_uint8("42")); }
void test_parse_uint8_zero(void)     { TEST_ASSERT_EQUAL_UINT8(0,   parse_uint8("0")); }
void test_parse_uint8_max(void)      { TEST_ASSERT_EQUAL_UINT8(255, parse_uint8("255")); }
void test_parse_uint8_overflow(void) { TEST_ASSERT_EQUAL_UINT8(0,   parse_uint8("256")); }

/* ── parse_uint16 ────────────────────────────────────────────────────────── */

void test_parse_uint16_nominal(void)   { TEST_ASSERT_EQUAL_UINT16(44180, parse_uint16("44180")); }
void test_parse_uint16_zero(void)      { TEST_ASSERT_EQUAL_UINT16(0,     parse_uint16("0")); }
void test_parse_uint16_max(void)       { TEST_ASSERT_EQUAL_UINT16(65535, parse_uint16("65535")); }
void test_parse_uint16_overflow(void)  { TEST_ASSERT_EQUAL_UINT16(0,     parse_uint16("65536")); }

/* ── parse_uint32 ────────────────────────────────────────────────────────── */

void test_parse_uint32_nominal(void) { TEST_ASSERT_EQUAL_UINT32(1000000,    parse_uint32("1000000")); }
void test_parse_uint32_zero(void)    { TEST_ASSERT_EQUAL_UINT32(0,          parse_uint32("0")); }
void test_parse_uint32_max(void)     { TEST_ASSERT_EQUAL_UINT32(4294967295U, parse_uint32("4294967295")); }

/* ── parse_uint64 ────────────────────────────────────────────────────────── */

void test_parse_uint64_large(void) {
    TEST_ASSERT_EQUAL_UINT64(UINT64_C(9000000000), parse_uint64("9000000000"));
}

/* ── main ────────────────────────────────────────────────────────────────── */

int main(void) {
    UNITY_BEGIN();

    RUN_TEST(test_parse_bool_true_literal);
    RUN_TEST(test_parse_bool_TRUE_upper);
    RUN_TEST(test_parse_bool_one);
    RUN_TEST(test_parse_bool_yes);
    RUN_TEST(test_parse_bool_YES_upper);
    RUN_TEST(test_parse_bool_false_literal);
    RUN_TEST(test_parse_bool_zero);
    RUN_TEST(test_parse_bool_empty);
    RUN_TEST(test_parse_bool_invalid);
    RUN_TEST(test_parse_bool_no);

    RUN_TEST(test_parse_uint8_nominal);
    RUN_TEST(test_parse_uint8_zero);
    RUN_TEST(test_parse_uint8_max);
    RUN_TEST(test_parse_uint8_overflow);

    RUN_TEST(test_parse_uint16_nominal);
    RUN_TEST(test_parse_uint16_zero);
    RUN_TEST(test_parse_uint16_max);
    RUN_TEST(test_parse_uint16_overflow);

    RUN_TEST(test_parse_uint32_nominal);
    RUN_TEST(test_parse_uint32_zero);
    RUN_TEST(test_parse_uint32_max);

    RUN_TEST(test_parse_uint64_large);

    return UNITY_END();
}
