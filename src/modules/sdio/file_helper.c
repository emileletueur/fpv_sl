
#include "file_helper.h"
#include "../../config/fpv_sl_config.h"
#include "../../utils/cast_from_str.h"
#include "debug_log.h"
#include "ff.h"
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

#define CONFIG_FILE_PATH "0:/default.conf"
static fpv_sl_conf_t fpv_sl_config = {.conf_is_loaded = false};

key_value_pair_t parse_conf_key_value(char *line) {
    key_value_pair_t result = {0};
    result.valid = false;

    char *space_pos = strchr(line, ' ');
    if (space_pos == NULL) {
        return result;
    }

    size_t first_len = space_pos - line;
    if (first_len >= sizeof(result.key)) {
        first_len = sizeof(result.value) - 1;
    }
    strncpy(result.key, line, first_len);
    result.key[first_len] = '\0';

    char *start = space_pos + 1;
    char *end = start;

    while (*end != '\0' && *end != '\n' && *end != '\r') {
        end++;
    }

    size_t second_len = end - start;
    if (second_len >= sizeof(result.value)) {
        second_len = sizeof(result.value) - 1;
    }
    strncpy(result.value, start, second_len);
    result.value[second_len] = '\0';

    result.valid = true;
    return result;
}

config_key_enum_t string_to_key_enum(const char *key) {
    if (strcmp(key, USE_ENABLE_PIN) == 0)
        return KEY_USE_ENABLE_PIN;
    if (strcmp(key, ALWAYS_RCD) == 0)
        return KEY_ALWAYS_RCD;
    if (strcmp(key, MIC_GAIN) == 0)
        return KEY_MIC_GAIN;
    if (strcmp(key, USE_HIGH_PASS_FILTER) == 0)
        return KEY_USE_HIGH_PASS_FILTER;
    if (strcmp(key, HIGH_PASS_CUTOFF_FREQ) == 0)
        return KEY_HIGH_PASS_CUTOFF_FREQ;
    if (strcmp(key, SAMPLE_RATE) == 0)
        return KEY_SAMPLE_RATE;
    if (strcmp(key, NEXT_FILE_NAME_INDEX) == 0)
        return KEY_NEXT_FILE_NAME_INDEX;
    if (strcmp(key, RCD_FOLDER) == 0)
        return KEY_RCD_FOLDER;
    if (strcmp(key, RCD_FILE_NAME) == 0)
        return KEY_RCD_FILE_NAME;
    if (strcmp(key, DEL_ON_MULTIPLE_ENABLE_TICK) == 0)
        return KEY_DEL_ON_MULTIPLE_ENABLE_TICK;
    return KEY_UNKNOWN;
}

bool read_line(char *buff, uint32_t buff_len, FIL *file_p) {
    return f_gets(buff, buff_len, file_p) != NULL;
}

const char *get_fresult_str(FRESULT res) {
    switch (res) {
    case FR_OK:
        return "OK";
    case FR_DISK_ERR:
        return "DISK_ERR - Low level disk I/O error";
    case FR_INT_ERR:
        return "INT_ERR - Assertion failed";
    case FR_NOT_READY:
        return "NOT_READY - Drive not ready";
    case FR_NO_FILE:
        return "NO_FILE - File not found";
    case FR_NO_PATH:
        return "NO_PATH - Path not found";
    case FR_INVALID_NAME:
        return "INVALID_NAME - Invalid path name";
    case FR_DENIED:
        return "DENIED - Access denied";
    case FR_EXIST:
        return "EXIST - File exists";
    case FR_INVALID_OBJECT:
        return "INVALID_OBJECT - Invalid object";
    case FR_WRITE_PROTECTED:
        return "WRITE_PROTECTED";
    case FR_INVALID_DRIVE:
        return "INVALID_DRIVE - Invalid drive number";
    case FR_NOT_ENABLED:
        return "NOT_ENABLED - Work area not mounted";
    case FR_NO_FILESYSTEM:
        return "NO_FILESYSTEM - No valid FAT volume";
    case FR_MKFS_ABORTED:
        return "MKFS_ABORTED";
    case FR_TIMEOUT:
        return "TIMEOUT";
    case FR_LOCKED:
        return "LOCKED - File locked";
    case FR_NOT_ENOUGH_CORE:
        return "NOT_ENOUGH_CORE - Not enough memory";
    case FR_TOO_MANY_OPEN_FILES:
        return "TOO_MANY_OPEN_FILES";
    case FR_INVALID_PARAMETER:
        return "INVALID_PARAMETER";
    default:
        return "UNKNOWN";
    }
}

const fpv_sl_conf_t* get_conf(void) {
    return &fpv_sl_config;
}

uint8_t read_conf_file(void) {
    FRESULT f_result;
    FATFS config_fatfs_p;
    FIL config_file_p;
    char line[64];

    LOGI("Read config file.\r\n");

    f_result = f_mount(&config_fatfs_p, "0:", 1);
    if (f_result != FR_OK) {
        LOGI("Mount failed: %d (%s)\r\n", f_result, get_fresult_str(f_result));
        return -1;
    }

    f_result = f_open(&config_file_p, CONFIG_FILE_PATH, FA_READ);
    if (f_result != FR_OK) {
        LOGI("Config file error: %d\r\n", f_result);
        return -1;
    }

    while (read_line(line, sizeof(line), &config_file_p)) {
        key_value_pair_t conf_item = parse_conf_key_value(line);
        LOGI("-> %s:%s\r\n", conf_item.key, conf_item.value);
        switch (string_to_key_enum(conf_item.key)) {
        case KEY_USE_ENABLE_PIN:
            fpv_sl_config.use_enable_pin = parse_bool(conf_item.value);
            break;
        case KEY_ALWAYS_RCD:
            fpv_sl_config.always_rcd = parse_bool(conf_item.value);
            break;
        case KEY_MIC_GAIN:
            fpv_sl_config.mic_gain = parse_uint8(conf_item.value);
            break;
        case KEY_USE_HIGH_PASS_FILTER:
            fpv_sl_config.use_high_pass_filter = parse_bool(conf_item.value);
            break;
        case KEY_HIGH_PASS_CUTOFF_FREQ:
            fpv_sl_config.high_pass_cutoff_freq = parse_uint8(conf_item.value);
            break;
        case KEY_SAMPLE_RATE:
            fpv_sl_config.sample_rate = parse_uint16(conf_item.value);
            break;
        case KEY_NEXT_FILE_NAME_INDEX:
            fpv_sl_config.next_file_name_index = parse_uint16(conf_item.value);
            break;
        case KEY_RCD_FOLDER:
            fpv_sl_config.rcd_folder = conf_item.value;
            break;
        case KEY_RCD_FILE_NAME:
            fpv_sl_config.rcd_file_name = conf_item.value;
            break;
        case KEY_DEL_ON_MULTIPLE_ENABLE_TICK:
            fpv_sl_config.delete_on_multiple_enable_tick = parse_bool(conf_item.value);
            break;
        case KEY_UNKNOWN:
            break;
        }
    }
    fpv_sl_config.conf_is_loaded = true;
    LOGI("Config loaded.\r\n");
    return 0;
}

uint8_t create_wav_file();
uint8_t append_wav_header();
uint8_t get_file_name();
uint8_t list_wav_files(void);
