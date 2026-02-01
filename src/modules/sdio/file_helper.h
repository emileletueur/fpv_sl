#pragma once

#include "ff.h"
#include "fpv_sl_config.h"
#include "file_helper.h"
#include <stdbool.h>
#include <stdint.h>

typedef struct {
    // RIFF Header
    char riff_header[4]; // "RIFF"
    uint32_t wav_size;   // File size - 8
    char wave_header[4]; // "WAVE"

    // Format Header
    char fmt_header[4];      // "fmt "
    uint32_t fmt_chunk_size; // 16 for PCM
    uint16_t audio_format;   // 1 for PCM
    uint16_t num_channels;   // 1 = Mono, 2 = Stereo
    uint32_t sample_rate;
    uint32_t byte_rate;   // sample_rate * num_channels * bits_per_sample/8
    uint16_t block_align; // num_channels * bits_per_sample/8
    uint16_t bits_per_sample;

    // Data Header
    char data_header[4]; // "data"
    uint32_t data_bytes; // Size of audio data
} wav_header_t;

const fpv_sl_conf_t *get_conf(void);
bool read_line(char *buff, uint32_t buff_len, FIL *file_p);
uint8_t read_conf_file(void);
int read_config_file(void);
uint8_t create_wav_file(void);
uint8_t append_wav_header();
uint8_t get_file_name();
uint8_t list_wav_files(void);
