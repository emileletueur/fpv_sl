#include "unity.h"
#include "fpv_sl_config.h"  /* déclare parse_conf_key_value et string_to_key_enum */
#include <string.h>

void setUp(void)    {}
void tearDown(void) {}

/* ── parse_conf_key_value ───────────────────────────────────────────────── */

/* Note : le séparateur clé/valeur dans le format réel est un espace ' ',
   pas un '=' (voir l'implémentation de parse_conf_key_value dans file_helper.c). */

void test_parse_valid_pair(void) {
    char line[] = "SAMPLE_RATE 44180";
    key_value_pair_t p = parse_conf_key_value(line);
    TEST_ASSERT_TRUE(p.valid);
    TEST_ASSERT_EQUAL_STRING("SAMPLE_RATE", p.key);
    TEST_ASSERT_EQUAL_STRING("44180", p.value);
}

void test_parse_no_space_separator_is_invalid(void) {
    char line[] = "SAMPLE_RATE44180";
    key_value_pair_t p = parse_conf_key_value(line);
    TEST_ASSERT_FALSE(p.valid);
}

void test_parse_empty_line_is_invalid(void) {
    char line[] = "";
    key_value_pair_t p = parse_conf_key_value(line);
    TEST_ASSERT_FALSE(p.valid);
}

void test_parse_strips_trailing_newline(void) {
    char line[] = "ALWAYS_RCD true\n";
    key_value_pair_t p = parse_conf_key_value(line);
    TEST_ASSERT_TRUE(p.valid);
    TEST_ASSERT_EQUAL_STRING("ALWAYS_RCD", p.key);
    TEST_ASSERT_EQUAL_STRING("true", p.value);
}

void test_parse_strips_trailing_crlf(void) {
    char line[] = "MIC_GAIN 80\r\n";
    key_value_pair_t p = parse_conf_key_value(line);
    TEST_ASSERT_TRUE(p.valid);
    TEST_ASSERT_EQUAL_STRING("MIC_GAIN", p.key);
    TEST_ASSERT_EQUAL_STRING("80", p.value);
}

void test_parse_bool_value_true(void) {
    char line[] = "USE_ENABLE_PIN true";
    key_value_pair_t p = parse_conf_key_value(line);
    TEST_ASSERT_TRUE(p.valid);
    TEST_ASSERT_EQUAL_STRING("USE_ENABLE_PIN", p.key);
    TEST_ASSERT_EQUAL_STRING("true", p.value);
}

void test_parse_bool_value_false(void) {
    char line[] = "USE_HIGH_PASS_FILTER false";
    key_value_pair_t p = parse_conf_key_value(line);
    TEST_ASSERT_TRUE(p.valid);
    TEST_ASSERT_EQUAL_STRING("false", p.value);
}

void test_parse_rcd_folder_value(void) {
    char line[] = "RCD_FOLDER records/";
    key_value_pair_t p = parse_conf_key_value(line);
    TEST_ASSERT_TRUE(p.valid);
    TEST_ASSERT_EQUAL_STRING("RCD_FOLDER", p.key);
    TEST_ASSERT_EQUAL_STRING("records/", p.value);
}

/* ── string_to_key_enum ─────────────────────────────────────────────────── */

void test_key_use_enable_pin(void) {
    TEST_ASSERT_EQUAL_INT(KEY_USE_ENABLE_PIN, string_to_key_enum("USE_ENABLE_PIN"));
}
void test_key_always_rcd(void) {
    TEST_ASSERT_EQUAL_INT(KEY_ALWAYS_RCD, string_to_key_enum("ALWAYS_RCD"));
}
void test_key_mic_gain(void) {
    TEST_ASSERT_EQUAL_INT(KEY_MIC_GAIN, string_to_key_enum("MIC_GAIN"));
}
void test_key_use_high_pass_filter(void) {
    TEST_ASSERT_EQUAL_INT(KEY_USE_HIGH_PASS_FILTER, string_to_key_enum("USE_HIGH_PASS_FILTER"));
}
void test_key_high_pass_cutoff_freq(void) {
    TEST_ASSERT_EQUAL_INT(KEY_HIGH_PASS_CUTOFF_FREQ, string_to_key_enum("HIGH_PASS_CUTOFF_FREQ"));
}
void test_key_sample_rate(void) {
    TEST_ASSERT_EQUAL_INT(KEY_SAMPLE_RATE, string_to_key_enum("SAMPLE_RATE"));
}
void test_key_buffer_size(void) {
    TEST_ASSERT_EQUAL_INT(KEY_BUFFER_SIZE, string_to_key_enum("BUFFER_SIZE"));
}
void test_key_is_mono_rcd(void) {
    TEST_ASSERT_EQUAL_INT(KEY_IS_MONO_RCD, string_to_key_enum("IS_MONO_RCD"));
}
void test_key_next_file_name_index(void) {
    TEST_ASSERT_EQUAL_INT(KEY_NEXT_FILE_NAME_INDEX, string_to_key_enum("NEXT_FILE_NAME_INDEX"));
}
void test_key_rcd_folder(void) {
    TEST_ASSERT_EQUAL_INT(KEY_RCD_FOLDER, string_to_key_enum("RCD_FOLDER"));
}
void test_key_rcd_file_name(void) {
    TEST_ASSERT_EQUAL_INT(KEY_RCD_FILE_NAME, string_to_key_enum("RCD_FILE_NAME"));
}
void test_key_del_on_multiple_enable_tick(void) {
    TEST_ASSERT_EQUAL_INT(KEY_DEL_ON_MULTIPLE_ENABLE_TICK,
                          string_to_key_enum("DEL_ON_MULTIPLE_ENABLE_TICK"));
}
void test_key_unknown_string(void) {
    TEST_ASSERT_EQUAL_INT(KEY_UNKNOWN, string_to_key_enum("FOOBAR"));
}
void test_key_empty_string(void) {
    TEST_ASSERT_EQUAL_INT(KEY_UNKNOWN, string_to_key_enum(""));
}
void test_key_case_sensitive(void) {
    /* Les clés sont sensibles à la casse : minuscules → KEY_UNKNOWN */
    TEST_ASSERT_EQUAL_INT(KEY_UNKNOWN, string_to_key_enum("sample_rate"));
    TEST_ASSERT_EQUAL_INT(KEY_UNKNOWN, string_to_key_enum("always_rcd"));
}

/* ── main ────────────────────────────────────────────────────────────────── */

int main(void) {
    UNITY_BEGIN();

    RUN_TEST(test_parse_valid_pair);
    RUN_TEST(test_parse_no_space_separator_is_invalid);
    RUN_TEST(test_parse_empty_line_is_invalid);
    RUN_TEST(test_parse_strips_trailing_newline);
    RUN_TEST(test_parse_strips_trailing_crlf);
    RUN_TEST(test_parse_bool_value_true);
    RUN_TEST(test_parse_bool_value_false);
    RUN_TEST(test_parse_rcd_folder_value);

    RUN_TEST(test_key_use_enable_pin);
    RUN_TEST(test_key_always_rcd);
    RUN_TEST(test_key_mic_gain);
    RUN_TEST(test_key_use_high_pass_filter);
    RUN_TEST(test_key_high_pass_cutoff_freq);
    RUN_TEST(test_key_sample_rate);
    RUN_TEST(test_key_buffer_size);
    RUN_TEST(test_key_is_mono_rcd);
    RUN_TEST(test_key_next_file_name_index);
    RUN_TEST(test_key_rcd_folder);
    RUN_TEST(test_key_rcd_file_name);
    RUN_TEST(test_key_del_on_multiple_enable_tick);
    RUN_TEST(test_key_unknown_string);
    RUN_TEST(test_key_empty_string);
    RUN_TEST(test_key_case_sensitive);

    return UNITY_END();
}
