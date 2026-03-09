
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

static fpv_sl_conf_t fpv_sl_config  = {.conf_is_loaded = false};
static char          s_record_folder[64]    = {0};
static char          s_record_prefix[64] = {0};

/* État de la pré-allocation en cours d'enregistrement. */
static bool     g_prealloc_active     = false;
static uint32_t g_audio_bytes_written = 0;

/* ── Valeurs par défaut ──────────────────────────────────────────────────── */

#define DEFAULT_RECORD_ON_BOOT                true
#define DEFAULT_USE_ENABLE_PIN            false
#define DEFAULT_MIC_GAIN                  100 /* 100 % = facteur 1.0× */
#define DEFAULT_USE_HIGH_PASS_FILTER      false
#define DEFAULT_HIGH_PASS_CUTOFF_FREQ     200
#define DEFAULT_USE_LOW_PASS_FILTER       false
#define DEFAULT_LOW_PASS_CUTOFF_FREQ      8000
#define DEFAULT_SAMPLE_RATE               22050
#define DEFAULT_BUFFER_SIZE               256
#define DEFAULT_MONO_RECORD               true
#define DEFAULT_FILE_INDEX      0
#define DEFAULT_RECORD_FOLDER                ""
#define DEFAULT_RECORD_PREFIX             "rec"
#define DEFAULT_DELETE_ON_TRIPLE_ARM    false
#define DEFAULT_MAX_RECORD_DURATION          300   /* 5 min ≈ 1 lipo */
#define DEFAULT_USE_UART_MSP              false
#define DEFAULT_MSP_UART_ID               1
#define DEFAULT_MSP_BAUD_RATE             115200
#define DEFAULT_MSP_ENABLE_CHANNEL        5
#define DEFAULT_MSP_CHANNEL_RANGE_MIN     1700
#define DEFAULT_MSP_CHANNEL_RANGE_MAX     2100
#define DEFAULT_MSP_LIPO_MIN_MV           3000
#define DEFAULT_TELEMETRY_ITEMS           TLM_RC  /* sticks CH1-8 uniquement */

static void apply_defaults(void) {
    fpv_sl_config.record_on_boot                 = DEFAULT_RECORD_ON_BOOT;
    fpv_sl_config.use_enable_pin             = DEFAULT_USE_ENABLE_PIN;
    fpv_sl_config.mic_gain                   = DEFAULT_MIC_GAIN;
    fpv_sl_config.use_high_pass_filter       = DEFAULT_USE_HIGH_PASS_FILTER;
    fpv_sl_config.high_pass_cutoff_freq      = DEFAULT_HIGH_PASS_CUTOFF_FREQ;
    fpv_sl_config.use_low_pass_filter        = DEFAULT_USE_LOW_PASS_FILTER;
    fpv_sl_config.low_pass_cutoff_freq       = DEFAULT_LOW_PASS_CUTOFF_FREQ;
    fpv_sl_config.sample_rate                = DEFAULT_SAMPLE_RATE;
    fpv_sl_config.buffer_size                = DEFAULT_BUFFER_SIZE;
    fpv_sl_config.mono_record                = DEFAULT_MONO_RECORD;
    fpv_sl_config.file_index       = DEFAULT_FILE_INDEX;
    fpv_sl_config.delete_on_triple_arm = DEFAULT_DELETE_ON_TRIPLE_ARM;
    fpv_sl_config.max_record_duration               = DEFAULT_MAX_RECORD_DURATION;
    fpv_sl_config.use_uart_msp                   = DEFAULT_USE_UART_MSP;
    fpv_sl_config.msp_uart_id                    = DEFAULT_MSP_UART_ID;
    fpv_sl_config.msp_baud_rate                  = DEFAULT_MSP_BAUD_RATE;
    fpv_sl_config.msp_enable_channel             = DEFAULT_MSP_ENABLE_CHANNEL;
    fpv_sl_config.msp_channel_range_min          = DEFAULT_MSP_CHANNEL_RANGE_MIN;
    fpv_sl_config.msp_channel_range_max          = DEFAULT_MSP_CHANNEL_RANGE_MAX;
    fpv_sl_config.msp_lipo_min_mv               = DEFAULT_MSP_LIPO_MIN_MV;
    fpv_sl_config.telemetry_items               = DEFAULT_TELEMETRY_ITEMS;
    strncpy(s_record_folder,    DEFAULT_RECORD_FOLDER,    sizeof(s_record_folder) - 1);
    strncpy(s_record_prefix, DEFAULT_RECORD_PREFIX, sizeof(s_record_prefix) - 1);
    fpv_sl_config.record_folder    = s_record_folder;
    fpv_sl_config.record_prefix = s_record_prefix;
}

static int8_t write_default_conf(void) {
    LOGI("Creating default.conf with factory defaults.");
    FRESULT fr = f_open(&file_p, CONFIG_FILE_PATH, FA_CREATE_ALWAYS | FA_WRITE);
    if (fr != FR_OK) {
        LOGE("write_default_conf: open failed: %d (%s).", fr, get_fresult_str(fr));
        return -1;
    }
    char buf[512];
    int len = snprintf(buf, sizeof(buf),
        RECORD_ON_BOOT                " %s\n"
        USE_ENABLE_PIN            " %s\n"
        MIC_GAIN                  " %u\n"
        USE_HIGH_PASS_FILTER      " %s\n"
        HIGH_PASS_CUTOFF_FREQ     " %u\n"
        USE_LOW_PASS_FILTER       " %s\n"
        LOW_PASS_CUTOFF_FREQ      " %u\n"
        SAMPLE_RATE               " %u\n"
        BUFFER_SIZE               " %u\n"
        MONO_RECORD               " %s\n"
        FILE_INDEX      " %u\n"
        RECORD_FOLDER                " %s\n"
        RECORD_PREFIX             " %s\n"
        DELETE_ON_TRIPLE_ARM " %s\n"
        MAX_RECORD_DURATION            " %u\n"
        USE_UART_MSP                " %s\n"
        MSP_UART_ID                 " %u\n"
        MSP_BAUD_RATE               " %lu\n"
        MSP_ENABLE_CHANNEL          " %u\n"
        MSP_CHANNEL_RANGE_MIN       " %u\n"
        MSP_CHANNEL_RANGE_MAX       " %u\n"
        MSP_LIPO_MIN_MV             " %u\n"
        TELEMETRY_ITEMS             " %u\n",
        DEFAULT_RECORD_ON_BOOT             ? "true" : "false",
        DEFAULT_USE_ENABLE_PIN         ? "true" : "false",
        DEFAULT_MIC_GAIN,
        DEFAULT_USE_HIGH_PASS_FILTER   ? "true" : "false",
        DEFAULT_HIGH_PASS_CUTOFF_FREQ,
        DEFAULT_USE_LOW_PASS_FILTER    ? "true" : "false",
        DEFAULT_LOW_PASS_CUTOFF_FREQ,
        DEFAULT_SAMPLE_RATE,
        DEFAULT_BUFFER_SIZE,
        DEFAULT_MONO_RECORD            ? "true" : "false",
        DEFAULT_FILE_INDEX,
        DEFAULT_RECORD_FOLDER,
        DEFAULT_RECORD_PREFIX,
        DEFAULT_DELETE_ON_TRIPLE_ARM ? "true" : "false",
        DEFAULT_MAX_RECORD_DURATION,
        DEFAULT_USE_UART_MSP           ? "true" : "false",
        DEFAULT_MSP_UART_ID,
        (unsigned long)DEFAULT_MSP_BAUD_RATE,
        DEFAULT_MSP_ENABLE_CHANNEL,
        DEFAULT_MSP_CHANNEL_RANGE_MIN,
        DEFAULT_MSP_CHANNEL_RANGE_MAX,
        DEFAULT_MSP_LIPO_MIN_MV,
        (unsigned)DEFAULT_TELEMETRY_ITEMS);
    if (len < 0 || (size_t)len >= sizeof(buf)) {
        LOGE("write_default_conf: snprintf overflow.");
        f_close(&file_p);
        return -1;
    }
    UINT bw;
    fr = f_write(&file_p, buf, (UINT)len, &bw);
    f_close(&file_p);
    if (fr != FR_OK || bw != (UINT)len) {
        LOGE("write_default_conf: write failed: %d (%s).", fr, get_fresult_str(fr));
        return -1;
    }
    LOGI("default.conf created (%d bytes).", len);
    return 0;
}

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
    if (strcmp(key, RECORD_ON_BOOT) == 0)
        return KEY_RECORD_ON_BOOT;
    if (strcmp(key, MIC_GAIN) == 0)
        return KEY_MIC_GAIN;
    if (strcmp(key, USE_HIGH_PASS_FILTER) == 0)
        return KEY_USE_HIGH_PASS_FILTER;
    if (strcmp(key, HIGH_PASS_CUTOFF_FREQ) == 0)
        return KEY_HIGH_PASS_CUTOFF_FREQ;
    if (strcmp(key, USE_LOW_PASS_FILTER) == 0)
        return KEY_USE_LOW_PASS_FILTER;
    if (strcmp(key, LOW_PASS_CUTOFF_FREQ) == 0)
        return KEY_LOW_PASS_CUTOFF_FREQ;
    if (strcmp(key, SAMPLE_RATE) == 0)
        return KEY_SAMPLE_RATE;
    if (strcmp(key, BUFFER_SIZE) == 0)
        return KEY_BUFFER_SIZE;
    if (strcmp(key, MONO_RECORD) == 0)
        return KEY_MONO_RECORD;
    if (strcmp(key, FILE_INDEX) == 0)
        return KEY_FILE_INDEX;
    if (strcmp(key, RECORD_FOLDER) == 0)
        return KEY_RECORD_FOLDER;
    if (strcmp(key, RECORD_PREFIX) == 0)
        return KEY_RECORD_PREFIX;
    if (strcmp(key, DELETE_ON_TRIPLE_ARM) == 0)
        return KEY_DELETE_ON_TRIPLE_ARM;
    if (strcmp(key, MAX_RECORD_DURATION) == 0)
        return KEY_MAX_RECORD_DURATION;
    if (strcmp(key, USE_UART_MSP) == 0)
        return KEY_USE_UART_MSP;
    if (strcmp(key, MSP_UART_ID) == 0)
        return KEY_MSP_UART_ID;
    if (strcmp(key, MSP_BAUD_RATE) == 0)
        return KEY_MSP_BAUD_RATE;
    if (strcmp(key, MSP_ENABLE_CHANNEL) == 0)
        return KEY_MSP_ENABLE_CHANNEL;
    if (strcmp(key, MSP_CHANNEL_RANGE_MIN) == 0)
        return KEY_MSP_CHANNEL_RANGE_MIN;
    if (strcmp(key, MSP_CHANNEL_RANGE_MAX) == 0)
        return KEY_MSP_CHANNEL_RANGE_MAX;
    if (strcmp(key, MSP_LIPO_MIN_MV) == 0)
        return KEY_MSP_LIPO_MIN_MV;
    if (strcmp(key, TELEMETRY_ITEMS) == 0)
        return KEY_TELEMETRY_ITEMS;
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

/* Construit le chemin final du fichier WAV : "0:/<record_folder><record_prefix><index>.wav" */
static void build_final_file_path(char *out, size_t out_size) {
    snprintf(out, out_size, "0:/%s%s%u.wav",
             fpv_sl_config.record_folder    ? fpv_sl_config.record_folder    : "",
             fpv_sl_config.record_prefix ? fpv_sl_config.record_prefix : "rec",
             fpv_sl_config.file_index);
}

/* Relit default.conf, remplace la ligne FILE_INDEX et réécrit le fichier. */
static int8_t update_next_file_index_in_conf(uint16_t new_index) {
    char read_buf[512];
    char write_buf[512];
    UINT br;

    LOGI("Updating %s to %u in conf.", FILE_INDEX, new_index);

    FRESULT fr = f_open(&file_p, CONFIG_FILE_PATH, FA_READ);
    if (fr != FR_OK) {
        LOGE("update_index: open failed: %d (%s).", fr, get_fresult_str(fr));
        return -1;
    }
    fr = f_read(&file_p, read_buf, sizeof(read_buf) - 1, &br);
    f_close(&file_p);
    if (fr != FR_OK) {
        LOGE("update_index: read failed: %d (%s).", fr, get_fresult_str(fr));
        return -1;
    }
    read_buf[br] = '\0';

    /* Localise la ligne FILE_INDEX et remplace la valeur. */
    const char *prefix = FILE_INDEX " ";
    char *pos = strstr(read_buf, prefix);
    if (pos == NULL) {
        LOGE("update_index: key not found in conf.");
        return -1;
    }
    char *eol       = strchr(pos, '\n');
    size_t before   = (size_t)(pos - read_buf);
    char new_line[32];
    int  nl_len     = snprintf(new_line, sizeof(new_line), "%s%u", prefix, new_index);
    if (nl_len < 0) return -1;
    size_t after_off = eol ? (size_t)(eol - read_buf) : (size_t)br;
    size_t after_len = (size_t)br - after_off;
    size_t total     = before + (size_t)nl_len + after_len;
    if (total >= sizeof(write_buf)) {
        LOGE("update_index: buffer overflow.");
        return -1;
    }
    memcpy(write_buf,                          read_buf,        before);
    memcpy(write_buf + before,                 new_line,        (size_t)nl_len);
    memcpy(write_buf + before + (size_t)nl_len, read_buf + after_off, after_len);
    write_buf[total] = '\0';

    fr = f_open(&file_p, CONFIG_FILE_PATH, FA_CREATE_ALWAYS | FA_WRITE);
    if (fr != FR_OK) {
        LOGE("update_index: write open failed: %d (%s).", fr, get_fresult_str(fr));
        return -1;
    }
    UINT bw;
    fr = f_write(&file_p, write_buf, total, &bw);
    f_close(&file_p);
    if (fr != FR_OK || bw != (UINT)total) {
        LOGE("update_index: write failed: %d (%s).", fr, get_fresult_str(fr));
        return -1;
    }

    fpv_sl_config.file_index = new_index;
    LOGI("Index updated to %u.", new_index);
    return 0;
}

int8_t read_conf_file(void) {
    FRESULT f_result;
    char line[64];

    LOGI("Read config file.");

    /* Pré-charger les valeurs par défaut : valides même si le fichier est absent. */
    apply_defaults();

    f_result = f_mount(&fatfs_mount_p, "0:", 1);
    if (f_result != FR_OK) {
        LOGE("Mount failed: %d (%s).", f_result, get_fresult_str(f_result));
        return -1;
    }

    f_result = f_open(&file_p, CONFIG_FILE_PATH, FA_READ);
    if (f_result == FR_NO_FILE) {
        LOGW("default.conf absent — using factory defaults.");
        write_default_conf();
        fpv_sl_config.conf_is_loaded = true;
        return 0;
    }
    if (f_result != FR_OK) {
        LOGE("Config file open error: %d (%s).", f_result, get_fresult_str(f_result));
        return -1;
    }

    while (read_line(line, sizeof(line), &file_p)) {
        key_value_pair_t conf_item = parse_conf_key_value(line);
        LOGI("Config item -> %s:%s.", conf_item.key, conf_item.value);
        switch (string_to_key_enum(conf_item.key)) {
        case KEY_USE_ENABLE_PIN:
            fpv_sl_config.use_enable_pin = parse_bool(conf_item.value);
            break;
        case KEY_RECORD_ON_BOOT:
            fpv_sl_config.record_on_boot = parse_bool(conf_item.value);
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
        case KEY_USE_LOW_PASS_FILTER:
            fpv_sl_config.use_low_pass_filter = parse_bool(conf_item.value);
            break;
        case KEY_LOW_PASS_CUTOFF_FREQ:
            fpv_sl_config.low_pass_cutoff_freq = parse_uint16(conf_item.value);
            break;
        case KEY_SAMPLE_RATE:
            fpv_sl_config.sample_rate = parse_uint16(conf_item.value);
            break;
        case KEY_BUFFER_SIZE:
            fpv_sl_config.buffer_size = parse_uint16(conf_item.value);
            break;
        case KEY_MONO_RECORD:
            fpv_sl_config.mono_record = parse_bool(conf_item.value);
            break;
        case KEY_FILE_INDEX:
            fpv_sl_config.file_index = parse_uint16(conf_item.value);
            break;
        case KEY_RECORD_FOLDER:
            strncpy(s_record_folder, conf_item.value, sizeof(s_record_folder) - 1);
            fpv_sl_config.record_folder = s_record_folder;
            break;
        case KEY_RECORD_PREFIX:
            strncpy(s_record_prefix, conf_item.value, sizeof(s_record_prefix) - 1);
            fpv_sl_config.record_prefix = s_record_prefix;
            break;
        case KEY_DELETE_ON_TRIPLE_ARM:
            fpv_sl_config.delete_on_triple_arm = parse_bool(conf_item.value);
            break;
        case KEY_MAX_RECORD_DURATION:
            fpv_sl_config.max_record_duration = parse_uint16(conf_item.value);
            break;
        case KEY_USE_UART_MSP:
            fpv_sl_config.use_uart_msp = parse_bool(conf_item.value);
            break;
        case KEY_MSP_UART_ID:
            fpv_sl_config.msp_uart_id = parse_uint8(conf_item.value);
            break;
        case KEY_MSP_BAUD_RATE:
            fpv_sl_config.msp_baud_rate = parse_uint32(conf_item.value);
            break;
        case KEY_MSP_ENABLE_CHANNEL:
            fpv_sl_config.msp_enable_channel = parse_uint8(conf_item.value);
            break;
        case KEY_MSP_CHANNEL_RANGE_MIN:
            fpv_sl_config.msp_channel_range_min = parse_uint16(conf_item.value);
            break;
        case KEY_MSP_CHANNEL_RANGE_MAX:
            fpv_sl_config.msp_channel_range_max = parse_uint16(conf_item.value);
            break;
        case KEY_MSP_LIPO_MIN_MV:
            fpv_sl_config.msp_lipo_min_mv = parse_uint16(conf_item.value);
            break;
        case KEY_TELEMETRY_ITEMS:
            fpv_sl_config.telemetry_items = parse_uint8(conf_item.value);
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

int8_t create_wav_file(void) {
    if (!fpv_sl_config.conf_is_loaded)
        return -1;

    char file_path[64];
    snprintf(file_path, sizeof(file_path), "0:/%s", TEMPORARY_FILE_NAME);

    FRESULT fr = f_open(&file_p, file_path, FA_CREATE_ALWAYS | FA_WRITE | FA_READ);
    if (fr != FR_OK) {
        LOGE("create_wav_file: open failed: %d (%s).", fr, get_fresult_str(fr));
        return -1;
    }

    g_audio_bytes_written = 0;
    g_prealloc_active     = false;

    /* Pré-allocation : réserve un bloc de clusters contigus pour éliminer la latence
       d'extension FAT pendant l'enregistrement.
       En cas d'échec (carte fragmentée, espace insuffisant), on continue sans pré-allocation —
       le recording reste fonctionnel, avec une latence d'écriture moins prévisible. */
    uint8_t  channels        = fpv_sl_config.mono_record ? 1 : 2;
    uint32_t pre_alloc_bytes = (uint32_t)fpv_sl_config.sample_rate
                               * channels * sizeof(int16_t)
                               * fpv_sl_config.max_record_duration
                               + sizeof(wav_header_t);
    LOGI("Requesting %lu B pre-allocation (%us at %uHz %s).",
         pre_alloc_bytes, fpv_sl_config.max_record_duration,
         fpv_sl_config.sample_rate, fpv_sl_config.mono_record ? "mono" : "stereo");

    fr = f_expand(&file_p, (FSIZE_t)pre_alloc_bytes, 1);
    if (fr == FR_OK) {
        g_prealloc_active = true;
        LOGI("Pre-allocation OK.");
    } else {
        LOGW("f_expand failed (%s) — recording without pre-allocation.", get_fresult_str(fr));
    }

    /* Header placeholder avec data_bytes = 0.
       Mis à jour à chaque sync_wav_file() pour permettre une récupération précise
       en cas de coupure secteur. */
    fr = f_lseek(&file_p, 0);
    if (fr != FR_OK) {
        LOGE("create_wav_file: seek failed: %d (%s).", fr, get_fresult_str(fr));
        f_close(&file_p);
        return -1;
    }
    wav_header_t placeholder = {.riff_header     = {'R', 'I', 'F', 'F'},
                                .wave_header     = {'W', 'A', 'V', 'E'},
                                .fmt_header      = {'f', 'm', 't', ' '},
                                .data_header     = {'d', 'a', 't', 'a'},
                                .fmt_chunk_size  = 16,
                                .audio_format    = 1,
                                .num_channels    = channels,
                                .sample_rate     = fpv_sl_config.sample_rate,
                                .bits_per_sample = 16,
                                .byte_rate       = fpv_sl_config.sample_rate * channels * sizeof(int16_t),
                                .block_align     = channels * sizeof(int16_t),
                                .wav_size        = 36,
                                .data_bytes      = 0};
    UINT bw;
    fr = f_write(&file_p, &placeholder, sizeof(placeholder), &bw);
    if (fr != FR_OK || bw != sizeof(placeholder)) {
        LOGE("create_wav_file: header write failed: %d (%s).", fr, get_fresult_str(fr));
        f_close(&file_p);
        return -1;
    }

    LOGI("WAV file created (pre-alloc: %s).", g_prealloc_active ? "yes" : "no");
    return 0;
}

int8_t finalize_wav_file(uint32_t rcd_duration) {
    FRESULT f_result;

    uint32_t data_bytes_real = g_audio_bytes_written;
    LOGI("Finalising WAV: %lu bytes of audio, duration %lums.", data_bytes_real, rcd_duration);

    /* 1. Truncate si pré-allocation active : libère les clusters non écrits.
          En cas d'échec on continue — le fichier sera trop grand mais l'audio sera valide. */
    if (g_prealloc_active) {
        f_result = f_lseek(&file_p, sizeof(wav_header_t) + data_bytes_real);
        if (f_result == FR_OK) {
            f_result = f_truncate(&file_p);
            if (f_result != FR_OK) {
                LOGW("Finalise: truncate failed (%s) — unused clusters not freed.", get_fresult_str(f_result));
            } else {
                LOGI("Pre-allocated space truncated.");
            }
        }
        g_prealloc_active = false;
    }

    /* 2. Réécriture du header avec les vraies tailles. */
    wav_header_t header = {.riff_header     = {'R', 'I', 'F', 'F'},
                           .wave_header     = {'W', 'A', 'V', 'E'},
                           .fmt_header      = {'f', 'm', 't', ' '},
                           .data_header     = {'d', 'a', 't', 'a'},
                           .fmt_chunk_size  = 16,
                           .audio_format    = 1,
                           .num_channels    = fpv_sl_config.mono_record ? 1 : 2,
                           .sample_rate     = fpv_sl_config.sample_rate,
                           .bits_per_sample = 16,
                           .byte_rate = fpv_sl_config.sample_rate * (fpv_sl_config.mono_record ? 1 : 2) * sizeof(int16_t),
                           .block_align     = (fpv_sl_config.mono_record ? 1 : 2) * sizeof(int16_t),
                           .wav_size        = 36 + data_bytes_real,
                           .data_bytes      = data_bytes_real};

    f_result = f_lseek(&file_p, 0);
    if (f_result != FR_OK) {
        LOGE("Finalise: seek failed: %d (%s).", f_result, get_fresult_str(f_result));
        return -1;
    }
    UINT bw;
    f_result = f_write(&file_p, &header, sizeof(header), &bw);
    if (f_result != FR_OK || bw != sizeof(header)) {
        LOGE("Finalise: header rewrite failed: %d (%s).", f_result, get_fresult_str(f_result));
        return -1;
    }

    /* 3. Fermeture. */
    f_result = f_close(&file_p);
    if (f_result != FR_OK) {
        LOGE("Finalise: close failed: %d (%s).", f_result, get_fresult_str(f_result));
    } else {
        LOGI("WAV file closed.");
    }

    /* 4. Renommage avec l'index courant. */
    char src_path[64];
    char dst_path[64];
    snprintf(src_path, sizeof(src_path), "0:/%s", TEMPORARY_FILE_NAME);
    build_final_file_path(dst_path, sizeof(dst_path));
    LOGI("Renaming to %s.", dst_path);
    f_result = f_rename(src_path, dst_path);
    if (f_result != FR_OK) {
        LOGE("Finalise: rename failed: %d (%s).", f_result, get_fresult_str(f_result));
        return -1;
    }

    /* 5. Incrément de l'index dans la conf. */
    if (update_next_file_index_in_conf(fpv_sl_config.file_index + 1) != 0) {
        LOGW("Finalise: index update failed.");
    }

    LOGI("Recording finalised.");
    return 0;
}

int8_t recover_unfinalized_recording(void) {
    char tmp_path[64];
    snprintf(tmp_path, sizeof(tmp_path), "0:/%s", TEMPORARY_FILE_NAME);

    LOGI("Checking for unfinalized recording.");
    FILINFO finfo;
    FRESULT fr = f_stat(tmp_path, &finfo);
    if (fr != FR_OK) {
        LOGI("No unfinalized recording found.");
        return 0;
    }

    LOGI("Unfinalized recording found (%lu bytes). Recovering.", (unsigned long)finfo.fsize);

    fr = f_open(&file_p, tmp_path, FA_READ | FA_WRITE);
    if (fr != FR_OK) {
        LOGE("recover: open failed: %d (%s).", fr, get_fresult_str(fr));
        return -1;
    }

    /* Lire le header WAV : sync_wav_file() le met à jour à chaque période (~370 ms).
       data_bytes reflète donc la quantité d'audio valide jusqu'au dernier sync.
       Si data_bytes == 0 (coupure avant le 1er sync) : fallback sur la taille fichier. */
    wav_header_t hdr = {0};
    UINT br;
    f_read(&file_p, &hdr, sizeof(hdr), &br);

    uint32_t data_bytes_real;
    if (hdr.data_bytes > 0 && hdr.data_bytes <= finfo.fsize - sizeof(wav_header_t)) {
        data_bytes_real = hdr.data_bytes;
        LOGI("recover: checkpoint found — %lu bytes of valid audio.", (unsigned long)data_bytes_real);
    } else {
        data_bytes_real = (finfo.fsize > sizeof(wav_header_t))
                              ? (uint32_t)(finfo.fsize - sizeof(wav_header_t))
                              : 0;
        LOGW("recover: no checkpoint — using full file size (%lu bytes, may include garbage).",
             (unsigned long)data_bytes_real);
    }

    /* Truncate au point de données réelles (libère les clusters pré-alloués non écrits). */
    FRESULT fr2 = f_lseek(&file_p, sizeof(wav_header_t) + data_bytes_real);
    if (fr2 == FR_OK) {
        fr2 = f_truncate(&file_p);
        if (fr2 != FR_OK) {
            LOGW("recover: truncate failed (%s) — unused clusters not freed.", get_fresult_str(fr2));
        }
    }

    wav_header_t header = {.riff_header     = {'R', 'I', 'F', 'F'},
                           .wave_header     = {'W', 'A', 'V', 'E'},
                           .fmt_header      = {'f', 'm', 't', ' '},
                           .data_header     = {'d', 'a', 't', 'a'},
                           .fmt_chunk_size  = 16,
                           .audio_format    = 1,
                           .num_channels    = fpv_sl_config.mono_record ? 1 : 2,
                           .sample_rate     = fpv_sl_config.sample_rate,
                           .bits_per_sample = 16,
                           .byte_rate = fpv_sl_config.sample_rate * (fpv_sl_config.mono_record ? 1 : 2) * sizeof(int16_t),
                           .block_align     = (fpv_sl_config.mono_record ? 1 : 2) * sizeof(int16_t),
                           .wav_size        = 36 + data_bytes_real,
                           .data_bytes      = data_bytes_real};

    fr = f_lseek(&file_p, 0);
    if (fr != FR_OK) {
        LOGE("recover: seek failed: %d (%s).", fr, get_fresult_str(fr));
        f_close(&file_p);
        return -1;
    }
    UINT bw;
    fr = f_write(&file_p, &header, sizeof(header), &bw);
    if (fr != FR_OK || bw != sizeof(header)) {
        LOGE("recover: header rewrite failed: %d (%s).", fr, get_fresult_str(fr));
        f_close(&file_p);
        return -1;
    }

    fr = f_close(&file_p);
    if (fr != FR_OK) {
        LOGE("recover: close failed: %d (%s).", fr, get_fresult_str(fr));
        return -1;
    }

    char dst_path[64];
    build_final_file_path(dst_path, sizeof(dst_path));
    LOGI("Renaming recovered file to %s.", dst_path);
    fr = f_rename(tmp_path, dst_path);
    if (fr != FR_OK) {
        LOGE("recover: rename failed: %d (%s).", fr, get_fresult_str(fr));
        return -1;
    }

    if (update_next_file_index_in_conf(fpv_sl_config.file_index + 1) != 0) {
        LOGW("recover: index update failed.");
    }

    LOGI("Recovery complete: %lu bytes of audio data.", (unsigned long)data_bytes_real);
    return 0;
}

int8_t sync_wav_file(void) {
    /* Checkpoint header : met à jour data_bytes avec la quantité d'audio déjà écrite.
       En cas de coupure secteur, recover_unfinalized_recording() lira ce champ pour
       reconstruire un fichier valide jusqu'au dernier sync (~370 ms de précision). */
    wav_header_t header = {.riff_header     = {'R', 'I', 'F', 'F'},
                           .wave_header     = {'W', 'A', 'V', 'E'},
                           .fmt_header      = {'f', 'm', 't', ' '},
                           .data_header     = {'d', 'a', 't', 'a'},
                           .fmt_chunk_size  = 16,
                           .audio_format    = 1,
                           .num_channels    = fpv_sl_config.mono_record ? 1 : 2,
                           .sample_rate     = fpv_sl_config.sample_rate,
                           .bits_per_sample = 16,
                           .byte_rate = fpv_sl_config.sample_rate * (fpv_sl_config.mono_record ? 1 : 2) * sizeof(int16_t),
                           .block_align     = (fpv_sl_config.mono_record ? 1 : 2) * sizeof(int16_t),
                           .wav_size        = 36 + g_audio_bytes_written,
                           .data_bytes      = g_audio_bytes_written};

    FRESULT fr = f_lseek(&file_p, 0);
    if (fr != FR_OK) {
        LOGE("sync: seek to header failed: %d (%s).", fr, get_fresult_str(fr));
        return -1;
    }
    UINT bw;
    fr = f_write(&file_p, &header, sizeof(header), &bw);
    if (fr != FR_OK || bw != sizeof(header)) {
        LOGE("sync: header checkpoint failed: %d (%s).", fr, get_fresult_str(fr));
        return -1;
    }

    /* Repositionnement au point d'écriture audio courant pour reprendre normalement. */
    fr = f_lseek(&file_p, sizeof(wav_header_t) + g_audio_bytes_written);
    if (fr != FR_OK) {
        LOGE("sync: seek restore failed: %d (%s).", fr, get_fresult_str(fr));
        return -1;
    }

    fr = f_sync(&file_p);
    if (fr != FR_OK) {
        LOGE("sync: f_sync failed: %d (%s).", fr, get_fresult_str(fr));
        return -1;
    }
    LOGD("WAV synced: %lu bytes.", g_audio_bytes_written);
    return 0;
}

int8_t get_disk_usage_percent(uint8_t *out_percent) {
    LOGI("Checking disk usage.");
    FATFS *fs;
    DWORD fre_clust;
    FRESULT f_result = f_getfree("0:", &fre_clust, &fs);
    if (f_result != FR_OK) {
        LOGE("f_getfree failed: %d (%s).", f_result, get_fresult_str(f_result));
        return -1;
    }
    DWORD total_clust = fs->n_fatent - 2;
    if (total_clust == 0) {
        LOGE("Disk total cluster count is zero.");
        return -1;
    }
    *out_percent = (uint8_t)(((total_clust - fre_clust) * 100UL) / total_clust);
    LOGI("Disk usage: %d%% (%lu / %lu clusters used).", *out_percent, total_clust - fre_clust, total_clust);
    return 0;
}

int8_t flush_audio_files(void) {
    DIR     dir;
    FILINFO fno;
    char    dir_path[64];
    char    file_path[320];

    const char *folder = fpv_sl_config.record_folder ? fpv_sl_config.record_folder : "";
    snprintf(dir_path, sizeof(dir_path), "0:/%s", folder);

    /* Supprime le '/' final si présent pour éviter les doubles slash dans file_path. */
    size_t dlen = strlen(dir_path);
    if (dlen > 3 && dir_path[dlen - 1] == '/')
        dir_path[dlen - 1] = '\0';

    LOGI("Flushing audio files from '%s'.", dir_path);

    FRESULT fr = f_opendir(&dir, dir_path);
    if (fr != FR_OK) {
        LOGE("flush_audio_files: f_opendir('%s') failed (%s).", dir_path, get_fresult_str(fr));
        return -1;
    }

    uint16_t deleted = 0;
    while (1) {
        fr = f_readdir(&dir, &fno);
        if (fr != FR_OK || fno.fname[0] == '\0')
            break;
        if (fno.fattrib & AM_DIR)
            continue;

        /* Ne supprime que les .wav (insensible à la casse — FatFS peut retourner majuscules). */
        size_t nlen = strlen(fno.fname);
        if (nlen < 5)
            continue;
        const char *ext = fno.fname + nlen - 4;
        if (!(ext[0] == '.' &&
              (ext[1] == 'W' || ext[1] == 'w') &&
              (ext[2] == 'A' || ext[2] == 'a') &&
              (ext[3] == 'V' || ext[3] == 'v')))
            continue;

        snprintf(file_path, sizeof(file_path), "%s/%s", dir_path, fno.fname);
        fr = f_unlink(file_path);
        if (fr == FR_OK) {
            LOGI("Deleted: %s", file_path);
            deleted++;
        } else {
            LOGW("f_unlink('%s') failed: %s", file_path, get_fresult_str(fr));
        }
    }
    f_closedir(&dir);

    LOGI("Flush complete: %u file(s) deleted.", deleted);

    if (update_next_file_index_in_conf(0) != 0)
        LOGW("flush_audio_files: index reset failed.");

    return 0;
}

int8_t write_buffer(uint32_t *buff) {
    uint32_t samples = fpv_sl_config.mono_record ? fpv_sl_config.buffer_size / 2
                                                 : fpv_sl_config.buffer_size;
    /* Conversion int32 (24-bit, ±2^23) → int16 (±2^15) : >> 8.
     * process_sample retourne la valeur audio INMP441 sur 24 bits après >> 8 sur le mot DMA. */
    static int16_t int16_buf[DEFAULT_BUFFER_SIZE];
    int32_t *ibuf = (int32_t *)buff;
    for (uint32_t i = 0; i < samples; i++) {
        int16_buf[i] = (int16_t)(ibuf[i] >> 8);
    }
    static uint32_t s_diag_wb = 0;
    if (++s_diag_wb % 172 == 0) {
        /* ~1s : log le premier int16 du bloc après conversion >>8 et le total écrit.
           Si int16[0] == 0 alors que raw DMA est non-nul → shift incorrect.
           Si int16[0] varie → pipeline OK. */
        LOGD("write_buf: int32[0]=%ld → int16[0]=%d  total=%lu B",
             (long)ibuf[0], int16_buf[0], (unsigned long)g_audio_bytes_written);
    }

    UINT bytes_to_write = samples * sizeof(int16_t);
    UINT bytes_written;
    FRESULT fr = f_write(&file_p, int16_buf, bytes_to_write, &bytes_written);
    if (fr != FR_OK || bytes_written != bytes_to_write) {
        LOGE("write_buffer: f_write failed: %d (%s).", fr, get_fresult_str(fr));
        return -1;
    }
    g_audio_bytes_written += bytes_written;
    return 0;
}
