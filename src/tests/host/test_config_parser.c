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
    char line[] = "RECORD_ON_BOOT true\n";
    key_value_pair_t p = parse_conf_key_value(line);
    TEST_ASSERT_TRUE(p.valid);
    TEST_ASSERT_EQUAL_STRING("RECORD_ON_BOOT", p.key);
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

void test_parse_record_folder_value(void) {
    char line[] = "RECORD_FOLDER records/";
    key_value_pair_t p = parse_conf_key_value(line);
    TEST_ASSERT_TRUE(p.valid);
    TEST_ASSERT_EQUAL_STRING("RECORD_FOLDER", p.key);
    TEST_ASSERT_EQUAL_STRING("records/", p.value);
}

/* ── string_to_key_enum ─────────────────────────────────────────────────── */

void test_key_use_enable_pin(void) {
    TEST_ASSERT_EQUAL_INT(KEY_USE_ENABLE_PIN, string_to_key_enum("USE_ENABLE_PIN"));
}
void test_key_record_on_boot(void) {
    TEST_ASSERT_EQUAL_INT(KEY_RECORD_ON_BOOT, string_to_key_enum("RECORD_ON_BOOT"));
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
void test_key_mono_record(void) {
    TEST_ASSERT_EQUAL_INT(KEY_MONO_RECORD, string_to_key_enum("MONO_RECORD"));
}
void test_key_file_index(void) {
    TEST_ASSERT_EQUAL_INT(KEY_FILE_INDEX, string_to_key_enum("FILE_INDEX"));
}
void test_key_record_folder(void) {
    TEST_ASSERT_EQUAL_INT(KEY_RECORD_FOLDER, string_to_key_enum("RECORD_FOLDER"));
}
void test_key_record_prefix(void) {
    TEST_ASSERT_EQUAL_INT(KEY_RECORD_PREFIX, string_to_key_enum("RECORD_PREFIX"));
}
void test_key_delete_on_triple_arm(void) {
    TEST_ASSERT_EQUAL_INT(KEY_DELETE_ON_TRIPLE_ARM,
                          string_to_key_enum("DELETE_ON_TRIPLE_ARM"));
}
void test_key_max_record_duration(void) {
    TEST_ASSERT_EQUAL_INT(KEY_MAX_RECORD_DURATION, string_to_key_enum("MAX_RECORD_DURATION"));
}
void test_key_use_uart_msp(void) {
    TEST_ASSERT_EQUAL_INT(KEY_USE_UART_MSP, string_to_key_enum("USE_UART_MSP"));
}
void test_key_msp_uart_id(void) {
    TEST_ASSERT_EQUAL_INT(KEY_MSP_UART_ID, string_to_key_enum("MSP_UART_ID"));
}
void test_key_msp_baud_rate(void) {
    TEST_ASSERT_EQUAL_INT(KEY_MSP_BAUD_RATE, string_to_key_enum("MSP_BAUD_RATE"));
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
    TEST_ASSERT_EQUAL_INT(KEY_UNKNOWN, string_to_key_enum("record_on_boot"));
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
    RUN_TEST(test_parse_record_folder_value);

    RUN_TEST(test_key_use_enable_pin);
    RUN_TEST(test_key_record_on_boot);
    RUN_TEST(test_key_mic_gain);
    RUN_TEST(test_key_use_high_pass_filter);
    RUN_TEST(test_key_high_pass_cutoff_freq);
    RUN_TEST(test_key_sample_rate);
    RUN_TEST(test_key_buffer_size);
    RUN_TEST(test_key_mono_record);
    RUN_TEST(test_key_file_index);
    RUN_TEST(test_key_record_folder);
    RUN_TEST(test_key_record_prefix);
    RUN_TEST(test_key_delete_on_triple_arm);
    RUN_TEST(test_key_max_record_duration);
    RUN_TEST(test_key_use_uart_msp);
    RUN_TEST(test_key_msp_uart_id);
    RUN_TEST(test_key_msp_baud_rate);
    RUN_TEST(test_key_unknown_string);
    RUN_TEST(test_key_empty_string);
    RUN_TEST(test_key_case_sensitive);

    return UNITY_END();
}
