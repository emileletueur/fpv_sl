
#include "file_helper.h"
#include "../../config/fpv_sl_config.h"
#include "../../usb/cdc/debug_cdc.h"
#include "../../utils/cast_from_str.h"
#include "ff.h"
#include <stdlib.h>
#include <string.h>

static fpv_sl_conf_t fpv_sl_config;

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

uint8_t read_conf_file(void) {
    FRESULT f_result;
    FIL *config_file_p;
    char *line = malloc(64);

    f_result = f_open(config_file_p, CONFIG_BASE_DIR "" CONFIG_FILE_NAME, FA_READ);
    if (f_result != FR_OK) {
        debug_cdc("Config file not found, using defaults\r\n");
        switch(f_result) {
        case FR_NO_FILE:
            debug_cdc("  -> File 'default.conf' not found in root\r\n");
            debug_cdc("  -> Check if file exists on SD card\r\n");
            break;
        case FR_NO_PATH:
            debug_cdc("  -> Path '0:/' not found\r\n");
            break;
        case FR_INVALID_NAME:
            debug_cdc("  -> Invalid filename\r\n");
            break;
        case FR_NOT_ENABLED:
            debug_cdc("  -> Drive not mounted!\r\n");
            break;
        case FR_NO_FILESYSTEM:
            debug_cdc("  -> No FAT filesystem found\r\n");
            break;
        default:
            debug_cdc("  -> Unknown error\r\n");
            break;
    }
        return -1;
    }

    while (read_line(line, sizeof(line), config_file_p)) {
        key_value_pair_t conf_item = parse_conf_key_value(line);
        debug_cdc(conf_item.key);
        debug_cdc(":");
        debug_cdc(conf_item.value);
        debug_cdc("\r\n");
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
    free(line);
    return 0;
}

uint8_t create_wav_file();
uint8_t append_wav_header();
uint8_t get_file_name();
uint8_t list_wav_files(void);
