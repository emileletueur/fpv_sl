#pragma once

#include <stdbool.h>
#include <stdint.h>

#ifndef CONFIG_FILE
#define CONFIG_BASE_DIR "0:/"
#define CONFIG_FILE_NAME "default.conf"
#define USE_ENABLE_PIN "USE_ENABLE_PIN"
#define RECORD_ON_BOOT "RECORD_ON_BOOT"
#define MIC_GAIN "MIC_GAIN"
#define USE_HIGH_PASS_FILTER "USE_HIGH_PASS_FILTER"
#define HIGH_PASS_CUTOFF_FREQ "HIGH_PASS_CUTOFF_FREQ"
#define USE_LOW_PASS_FILTER "USE_LOW_PASS_FILTER"
#define LOW_PASS_CUTOFF_FREQ "LOW_PASS_CUTOFF_FREQ"
#define SAMPLE_RATE "SAMPLE_RATE"
#define BUFFER_SIZE "BUFFER_SIZE"
#define MONO_RECORD "MONO_RECORD"
#define FILE_INDEX "FILE_INDEX"
#define RECORD_FOLDER "RECORD_FOLDER"
#define RECORD_PREFIX "RECORD_PREFIX"
#define DELETE_ON_TRIPLE_ARM "DELETE_ON_TRIPLE_ARM"
#define MAX_RECORD_DURATION "MAX_RECORD_DURATION"
#define USE_UART_MSP "USE_UART_MSP"
#define MSP_UART_ID "MSP_UART_ID"
#define MSP_BAUD_RATE "MSP_BAUD_RATE"
#define MSP_ENABLE_CHANNEL    "MSP_ENABLE_CHANNEL"
#define MSP_CHANNEL_RANGE_MIN "MSP_CHANNEL_RANGE_MIN"
#define MSP_CHANNEL_RANGE_MAX "MSP_CHANNEL_RANGE_MAX"
#define MSP_LIPO_MIN_MV       "MSP_LIPO_MIN_MV"
#define TELEMETRY_ITEMS       "TELEMETRY_ITEMS"
#endif

/* Bitmask des sources de télémétrie enregistrables.
   Valeur stockée dans fpv_sl_conf_t.telemetry_items et dans le header .tlm. */
typedef enum {
    TLM_RC       = (1u << 0), /* MSP_RC 105       — CH1-8 (uint16 × 8, 16 B) */
    TLM_ATTITUDE = (1u << 1), /* MSP_ATTITUDE 108 — roll/pitch/yaw (int16 × 3, 6 B) */
    TLM_GPS      = (1u << 2), /* MSP_RAW_GPS 106  — fix/sats/lat/lon/alt/speed (14 B) */
    TLM_ANALOG   = (1u << 3), /* MSP_ANALOG 110   — vbat/mAh/rssi/courant (7 B) */
} tlm_item_flags_t;

typedef struct {
    char key[64];
    char value[64];
    bool valid;
} key_value_pair_t;

typedef enum {
    KEY_UNKNOWN = 0,
    KEY_USE_ENABLE_PIN,
    KEY_RECORD_ON_BOOT,
    KEY_MIC_GAIN,
    KEY_USE_HIGH_PASS_FILTER,
    KEY_HIGH_PASS_CUTOFF_FREQ,
    KEY_USE_LOW_PASS_FILTER,
    KEY_LOW_PASS_CUTOFF_FREQ,
    KEY_SAMPLE_RATE,
    KEY_BUFFER_SIZE,
    KEY_MONO_RECORD,
    KEY_FILE_INDEX,
    KEY_RECORD_FOLDER,
    KEY_RECORD_PREFIX,
    KEY_DELETE_ON_TRIPLE_ARM,
    KEY_MAX_RECORD_DURATION,
    KEY_USE_UART_MSP,
    KEY_MSP_UART_ID,
    KEY_MSP_BAUD_RATE,
    KEY_MSP_ENABLE_CHANNEL,
    KEY_MSP_CHANNEL_RANGE_MIN,
    KEY_MSP_CHANNEL_RANGE_MAX,
    KEY_MSP_LIPO_MIN_MV,
    KEY_TELEMETRY_ITEMS,
} config_key_enum_t;

typedef struct {
    bool conf_is_loaded;
    bool use_enable_pin;           // record only be triggerd by arm/desarm pin
    bool record_on_boot;               // record as soon as module is powered
    uint8_t mic_gain;              // gain en pourcentage : 80 = 0.8×, 100 = 1.0×, 200 = 2.0×
    bool use_high_pass_filter;      // enable the high-pass filter
    uint8_t high_pass_cutoff_freq;  // HP cutoff frequency in Hz (≤ 255)
    bool use_low_pass_filter;       // enable the low-pass filter
    uint16_t low_pass_cutoff_freq;  // LP cutoff frequency in Hz
    uint16_t sample_rate;          // i2s rcd sample rate
    uint16_t buffer_size;          // i2s buffer size
    bool mono_record;          // i2s rcd sample rate
    uint16_t file_index; // the index used in file name to ensure unicity
    char *record_folder;
    char *record_prefix;
    bool delete_on_triple_arm;
    uint16_t max_record_duration;  /* durée max d'enregistrement en secondes (pré-allocation) */
    bool     use_uart_msp;          /* active le polling MSP via UART pour le trigger ARM */
    uint8_t  msp_uart_id;           /* UART Pico utilisé pour MSP : 0 ou 1 */
    uint32_t msp_baud_rate;         /* baud rate MSP : 115200 / 230400 / 460800 */
    uint8_t  msp_enable_channel;    /* canal RC 1-based pour le trigger ENABLE (ex. AUX1 = 5) */
    uint16_t msp_channel_range_min; /* µs — début de la plage active du canal */
    uint16_t msp_channel_range_max; /* µs — fin de la plage active du canal */
    uint16_t msp_lipo_min_mv;       /* tension minimale LiPo en mV (ex. 3000 = 3 V/cell) */
    uint8_t  telemetry_items;       /* bitmask tlm_item_flags_t — 0 = désactivé */
} fpv_sl_conf_t;

key_value_pair_t parse_conf_key_value(char *line);
config_key_enum_t string_to_key_enum(const char *key);
