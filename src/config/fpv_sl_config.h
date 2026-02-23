#pragma once

#include <stdbool.h>
#include <stdint.h>

#ifndef CONFIG_FILE
#define CONFIG_BASE_DIR "0:/"
#define CONFIG_FILE_NAME "default.conf"
#define USE_ENABLE_PIN "USE_ENABLE_PIN"
#define ALWAYS_RCD "ALWAYS_RCD"
#define MIC_GAIN "MIC_GAIN"
#define USE_HIGH_PASS_FILTER "USE_HIGH_PASS_FILTER"
#define HIGH_PASS_CUTOFF_FREQ "HIGH_PASS_CUTOFF_FREQ"
#define SAMPLE_RATE "SAMPLE_RATE"
#define BUFFER_SIZE "BUFFER_SIZE"
#define IS_MONO_RCD "IS_MONO_RCD"
#define NEXT_FILE_NAME_INDEX "NEXT_FILE_NAME_INDEX"
#define RCD_FOLDER "RCD_FOLDER"
#define RCD_FILE_NAME "RCD_FILE_NAME"
#define DEL_ON_MULTIPLE_ENABLE_TICK "DEL_ON_MULTIPLE_ENABLE_TICK"
#endif

typedef struct {
    char key[64];
    char value[64];
    bool valid;
} key_value_pair_t;

typedef enum {
    KEY_UNKNOWN = 0,
    KEY_USE_ENABLE_PIN,
    KEY_ALWAYS_RCD,
    KEY_MIC_GAIN,
    KEY_USE_HIGH_PASS_FILTER,
    KEY_HIGH_PASS_CUTOFF_FREQ,
    KEY_SAMPLE_RATE,
    KEY_BUFFER_SIZE,
    KEY_IS_MONO_RCD,
    KEY_NEXT_FILE_NAME_INDEX,
    KEY_RCD_FOLDER,
    KEY_RCD_FILE_NAME,
    KEY_DEL_ON_MULTIPLE_ENABLE_TICK,
} config_key_enum_t;

typedef struct {
    bool conf_is_loaded;
    bool use_enable_pin;           // record only be triggerd by arm/desarm pin
    bool always_rcd;               // record as soon as module is powered
    uint8_t mic_gain;              // use to tune mic gain
    bool use_high_pass_filter;     // use the low pass filter
    uint8_t high_pass_cutoff_freq; // the cutoff frequency of numeric low pass filter
    uint16_t sample_rate;          // i2s rcd sample rate
    uint16_t buffer_size;          // i2s buffer size
    bool is_mono_rcd;          // i2s rcd sample rate
    uint16_t next_file_name_index; // the index used in file name to ensure unicity
    char *rcd_folder;
    char *rcd_file_name;
    bool delete_on_multiple_enable_tick;
} fpv_sl_conf_t;

key_value_pair_t parse_conf_key_value(char *line);
config_key_enum_t string_to_key_enum(const char *key);
