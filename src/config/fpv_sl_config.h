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
    KEY_NEXT_FILE_NAME_INDEX,
    KEY_RCD_FOLDER,
    KEY_RCD_FILE_NAME,
    KEY_DEL_ON_MULTIPLE_ENABLE_TICK,
} config_key_enum_t;

key_value_pair_t parse_conf_key_value(char *line);
config_key_enum_t string_to_key_enum(const char *key);
