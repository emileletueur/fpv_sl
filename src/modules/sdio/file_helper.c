
#include "file_helper.h"
#include "../../config/fpv_sl_config.h"
#include "../../utils/cast_from_str.h"
#include "debug_log.h"
#include "ff.h"
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CONFIG_FILE_PATH "0:/default.conf"
#define TEMPORARY_FILE_NAME "t_mic_rcd.wav"

FATFS fatfs_mount_p;
FIL file_p;

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
    if (strcmp(key, IS_MONO_RCD) == 0)
        return KEY_IS_MONO_RCD;
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

const fpv_sl_conf_t *get_conf(void) {
    return &fpv_sl_config;
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

uint8_t read_conf_file(void) {
    FRESULT f_result;
    char line[64];

    LOGI("Read config file.");

    f_result = f_mount(&fatfs_mount_p, "0:", 1);
    if (f_result != FR_OK) {
        LOGI("Mount failed: %d (%s).", f_result, get_fresult_str(f_result));
        return -1;
    }

    f_result = f_open(&file_p, CONFIG_FILE_PATH, FA_READ);
    if (f_result != FR_OK) {
        LOGI("Config file error: %d.", f_result);
        return -1;
    }

    while (read_line(line, sizeof(line), &file_p)) {
        key_value_pair_t conf_item = parse_conf_key_value(line);
        LOGI("Config item -> %s:%s.", conf_item.key, conf_item.value);
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
        case KEY_IS_MONO_RCD:
            fpv_sl_config.is_mono_rcd = parse_uint16(conf_item.value);
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
    LOGI("Config loaded.");

    f_result = f_close(&file_p);
    if (f_result != FR_OK) {
        LOGI("Close file err: %d.", f_result);
    } else {
        LOGI("File closed succesfully.");
    }
    return 0;
}

uint8_t create_wav_file(void) {
    if (!fpv_sl_config.conf_is_loaded)
        return -1;
    char file_path[64];
    snprintf(file_path, sizeof(file_path), "0:/%s", TEMPORARY_FILE_NAME);
    FRESULT f_result = f_open(&file_p, file_path, FA_CREATE_ALWAYS | FA_WRITE | FA_READ);
    if (f_result != FR_OK) {
        LOGI("Failed to create temporary file: %d", f_result);
    }

    return 0;
}

uint8_t finalize_wav_file(uint32_t rcd_duration) {
    FRESULT f_result;
    f_result = f_close(&file_p);
    if (f_result != FR_OK) {
        LOGI("Close wav file err: %d.", f_result);
    } else {
        LOGI("WAV file finalised succesfully.");
    }

    char original_file_path_name[64];
    char final_file_path_name[64];
    char time_str[6];
    uint32_t seconds = rcd_duration / 1000;
    uint32_t minutes = seconds / 60;
    seconds %= 60;

    snprintf(time_str, 6, "%02u-%02u", minutes, seconds);

    snprintf(original_file_path_name, sizeof(original_file_path_name), "0:/%s", TEMPORARY_FILE_NAME);
    snprintf(final_file_path_name, sizeof(final_file_path_name), "0:/%s%s", fpv_sl_config.rcd_folder, time_str);
    f_result = f_rename(original_file_path_name, final_file_path_name);
    if (f_result != FR_OK) {
        LOGI("Move wav file err: %d.", f_result);
    } else {
        LOGI("WAV file moved in destination folder succesfully.");
    }
    return 0;
}

uint8_t append_wav_header(uint32_t data_size) {
    FRESULT f_result;
    UINT bytes_written;
    wav_header_t header = {.riff_header = {'R', 'I', 'F', 'F'},
                           .wave_header = {'W', 'A', 'V', 'E'},
                           .fmt_header = {'f', 'm', 't', ' '},
                           .data_header = {'D', 'A', 'T', 'A'},
                           .fmt_chunk_size = 16,
                           .audio_format = 1,
                           .num_channels = fpv_sl_config.is_mono_rcd ? 1 : 2,
                           .sample_rate = fpv_sl_config.sample_rate,
                           .bits_per_sample = 16,
                           .byte_rate = fpv_sl_config.sample_rate * (fpv_sl_config.is_mono_rcd ? 1 : 2) * 16 / 8,
                           .block_align = (fpv_sl_config.is_mono_rcd ? 1 : 2) * 16 / 8,
                           .wav_size = 36 + 1000,
                           .data_bytes = 1000};

    f_result = f_lseek(&file_p, 0);
    if (f_result != FR_OK || bytes_written != sizeof(header)) {
        // Gestion d'erreur
        LOGI("Err while seek 0 the file : %d", f_result);
    }

    f_result = f_write(&file_p, &header, sizeof(header), &bytes_written);
    if (f_result != FR_OK || bytes_written != sizeof(header)) {
        // Gestion d'erreur
        LOGI("Err while write header : %d", f_result);
    }

    return 0;
}
