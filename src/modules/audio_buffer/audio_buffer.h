#ifndef AUDIO_BUFFER_H
#define AUDIO_BUFFER_H

#include <stdint.h>
#include <stdbool.h>
#include "pico/critical_section.h"

// ─────────────────────────────────────────────
// Dimensionnement
// ─────────────────────────────────────────────
// INMP441 : 24-bit dans un mot 32-bit, mono
// Sample rate : 16000 Hz (ou 44100, à ajuster)
// DMA block : 256 samples = 1024 bytes
// Durée d'un bloc à 16kHz : 256/16000 = 16ms
// Nombre de blocs : 8 → 128ms de marge
// ─────────────────────────────────────────────

#define AUDIO_SAMPLE_RATE       44180
#define AUDIO_BLOCK_SAMPLES     256
#define AUDIO_BLOCK_SIZE_BYTES  (AUDIO_BLOCK_SAMPLES * sizeof(int32_t))
#define AUDIO_BLOCK_COUNT       8   // Ring buffer depth

typedef enum {
    BLOCK_FREE,         // DMA peut écrire
    BLOCK_DMA_FILLING,  // DMA en cours d'écriture
    BLOCK_READY,        // Prêt pour traitement
    BLOCK_PROCESSING,   // Core 0 traite
    BLOCK_WRITING,      // Core 1 écrit sur SD
} block_state_t;

typedef struct {
    int32_t samples[AUDIO_BLOCK_SAMPLES];
} __attribute__((aligned(4))) audio_block_t;

typedef struct {
    // Ring buffer de blocs audio
    audio_block_t           blocks[AUDIO_BLOCK_COUNT];
    volatile block_state_t  state[AUDIO_BLOCK_COUNT];

    // Index circulaires
    volatile uint8_t  dma_write_idx;    // Prochain bloc pour DMA
    volatile uint8_t  process_read_idx; // Prochain bloc à traiter
    volatile uint8_t  sd_write_idx;     // Prochain bloc à écrire SD

    // Statistiques / debug
    volatile uint32_t overrun_count;    // Blocs perdus
    volatile uint32_t blocks_captured;
    volatile uint32_t blocks_written;

    // Synchronisation inter-core
    critical_section_t crit_sec;
} audio_pipeline_t;

// ─────────────────────────────────────────────
// API
// ─────────────────────────────────────────────

void     audio_pipeline_init(audio_pipeline_t *pipeline);

// Appelé par IRQ DMA (depuis i2s_mic)
int32_t* audio_pipeline_get_dma_buffer(audio_pipeline_t *pipeline);
void     audio_pipeline_dma_complete(audio_pipeline_t *pipeline);

// Appelé par Core 0 (traitement)
bool     audio_pipeline_process_available(audio_pipeline_t *pipeline);
int32_t* audio_pipeline_get_process_buffer(audio_pipeline_t *pipeline);
void     audio_pipeline_process_done(audio_pipeline_t *pipeline);

// Appelé par Core 1 (écriture SD)
bool     audio_pipeline_write_available(audio_pipeline_t *pipeline);
int32_t* audio_pipeline_get_write_buffer(audio_pipeline_t *pipeline);
void     audio_pipeline_write_done(audio_pipeline_t *pipeline);

// Debug
uint32_t audio_pipeline_get_overruns(audio_pipeline_t *pipeline);
uint8_t  audio_pipeline_get_pending_count(audio_pipeline_t *pipeline);

#endif // AUDIO_BUFFER_H
