#pragma once

#include "ff.h"
#include <stdbool.h>
#include <stdint.h>


typedef struct {
    bool use_enable_pin;           // record only be triggerd by arm/desarm pin
    bool always_rcd;               // record as soon as module is powered
    uint8_t mic_gain;              // use to tune mic gain
    bool use_high_pass_filter;      // use the low pass filter
    uint8_t high_pass_cutoff_freq;  // the cutoff frequency of numeric low pass filter
    uint16_t sample_rate;          // i2s rcd sample rate
    uint16_t next_file_name_index; // the index used in file name to ensure unicity
    char *rcd_folder;
    char *rcd_file_name;
    bool delete_on_multiple_enable_tick;
} fpv_sl_conf_t;

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

#if FF_MULTI_PARTITION
// Partition table: {Physical drive, Partition index}
PARTITION VolToPart[] = {
    {0, 0},    // Logical drive 0 ==> Physical drive 0, Auto detect
    {0, 1},    // Logical drive 1 ==> Physical drive 0, Partition 1
    {0, 2},    // Logical drive 2 ==> Physical drive 0, Partition 2
    {0, 3},    // Logical drive 3 ==> Physical drive 0, Partition 3
    {1, 0},    // Logical drive 4 ==> Physical drive 1, Auto detect
};
#endif

bool read_line(char *buff, uint32_t buff_len, FIL *file_p);
uint8_t read_conf_file(void);
uint8_t create_wav_file();
uint8_t append_wav_header();
uint8_t get_file_name();
uint8_t list_wav_files(void);
