#pragma once

#include "ff.h"
#include "file_helper.h"
#include "fpv_sl_config.h"
#include <stdbool.h>
#include <stdint.h>

#pragma pack(push, 1)
typedef struct {
    const char riff_header[4]; // "RIFF"
    uint32_t wav_size;         // File size - 8
    const char wave_header[4]; // "WAVE"
    char fmt_header[4];        // "fmt "
    uint32_t fmt_chunk_size;   // 16 for PCM
    uint16_t audio_format;     // 1 for PCM
    uint16_t num_channels;     // 1 = Mono, 2 = Stereo
    uint32_t sample_rate;
    uint32_t byte_rate;   // sample_rate * num_channels * bits_per_sample/8
    uint16_t block_align; // num_channels * bits_per_sample/8
    uint16_t bits_per_sample;
    char data_header[4]; // "data"
    uint32_t data_bytes; // Size of audio data
} wav_header_t;
#pragma pack(pop)

const fpv_sl_conf_t *get_conf(void);
bool read_line(char *buff, uint32_t buff_len, FIL *file_p);
int8_t read_conf_file(void);
int read_config_file(void);
int8_t create_wav_file(void);
int8_t append_wav_header(uint32_t data_size);
int8_t finalize_wav_file(uint32_t rcd_duration);
int8_t write_buffer(uint32_t *buff);

/* Détecte t_mic_rcd.wav (sentinel d'enregistrement non finalisé), réécrit son header WAV
   avec les vraies tailles, le renomme avec next_file_name_index et incrémente l'index.
   Retourne 0 si aucune récupération nécessaire ou si la récupération réussit, -1 sinon.
   Doit être appelé après read_conf_file(). */
int8_t recover_unfinalized_recording(void);

/* Appelle f_sync() sur le fichier WAV en cours d'enregistrement.
   À appeler périodiquement depuis fpv_sl_core pour limiter les pertes en cas de coupure. */
int8_t sync_wav_file(void);

/* Retourne le pourcentage d'espace utilisé sur le volume monté (0-100).
   Retourne -1 si f_getfree échoue. Doit être appelé après montage (read_conf_file). */
int8_t get_disk_usage_percent(uint8_t *out_percent);
