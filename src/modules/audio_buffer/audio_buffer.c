#include "audio_buffer.h"
#include "pico/critical_section.h"
#include <string.h>

// ─────────────────────────────────────────────
// Helpers
// ─────────────────────────────────────────────

static inline uint8_t next_idx(uint8_t idx) {
    return (idx + 1) % AUDIO_BLOCK_COUNT;
}

// ─────────────────────────────────────────────
// Init
// ─────────────────────────────────────────────

void audio_pipeline_init(audio_pipeline_t *pipeline) {
    memset(pipeline, 0, sizeof(audio_pipeline_t));

    for (int i = 0; i < AUDIO_BLOCK_COUNT; i++) {
        pipeline->state[i] = BLOCK_FREE;
    }

    pipeline->dma_write_idx    = 0;
    pipeline->process_read_idx = 0;
    pipeline->sd_write_idx     = 0;
    pipeline->overrun_count    = 0;
    pipeline->blocks_captured  = 0;
    pipeline->blocks_written   = 0;

    critical_section_init(&pipeline->crit_sec);
}

// ─────────────────────────────────────────────
// DMA side (appelé depuis IRQ)
// ─────────────────────────────────────────────

// Retourne le pointeur vers le buffer que le DMA doit remplir
// Appelé AVANT de configurer le DMA transfer
int32_t* audio_pipeline_get_dma_buffer(audio_pipeline_t *pipeline) {
    uint8_t idx = pipeline->dma_write_idx;

    // Vérifier si le bloc est libre
    if (pipeline->state[idx] != BLOCK_FREE) {
        // ⚠️ OVERRUN : le pipeline n'a pas consommé assez vite
        // On écrase le plus ancien bloc non encore traité
        pipeline->overrun_count++;

        // Forcer la libération (on perd des données)
        pipeline->state[idx] = BLOCK_FREE;
    }

    pipeline->state[idx] = BLOCK_DMA_FILLING;
    return pipeline->blocks[idx].samples;
}

// Appelé dans l'IRQ DMA quand le transfert est terminé
void audio_pipeline_dma_complete(audio_pipeline_t *pipeline) {
    uint8_t idx = pipeline->dma_write_idx;

    pipeline->state[idx] = BLOCK_READY;
    pipeline->blocks_captured++;

    // Avancer l'index d'écriture DMA
    pipeline->dma_write_idx = next_idx(idx);
}

// ─────────────────────────────────────────────
// Core 0 : Traitement (filtrage, gain, etc.)
// ─────────────────────────────────────────────

bool audio_pipeline_process_available(audio_pipeline_t *pipeline) {
    return pipeline->state[pipeline->process_read_idx] == BLOCK_READY;
}

int32_t* audio_pipeline_get_process_buffer(audio_pipeline_t *pipeline) {
    uint8_t idx = pipeline->process_read_idx;

    if (pipeline->state[idx] != BLOCK_READY) {
        return NULL;
    }

    pipeline->state[idx] = BLOCK_PROCESSING;
    return pipeline->blocks[idx].samples;
}

void audio_pipeline_process_done(audio_pipeline_t *pipeline) {
    uint8_t idx = pipeline->process_read_idx;

    // Le bloc passe en attente d'écriture SD
    // (pas FREE, car Core 1 doit encore l'écrire)
    pipeline->state[idx] = BLOCK_WRITING;

    pipeline->process_read_idx = next_idx(idx);
}

// ─────────────────────────────────────────────
// Core 1 : Écriture SD
// ─────────────────────────────────────────────

bool audio_pipeline_write_available(audio_pipeline_t *pipeline) {
    return pipeline->state[pipeline->sd_write_idx] == BLOCK_WRITING;
}

int32_t* audio_pipeline_get_write_buffer(audio_pipeline_t *pipeline) {
    uint8_t idx = pipeline->sd_write_idx;

    if (pipeline->state[idx] != BLOCK_WRITING) {
        return NULL;
    }

    return pipeline->blocks[idx].samples;
}

void audio_pipeline_write_done(audio_pipeline_t *pipeline) {
    uint8_t idx = pipeline->sd_write_idx;

    // ✅ Le bloc est maintenant libre pour le DMA
    pipeline->state[idx] = BLOCK_FREE;
    pipeline->blocks_written++;

    pipeline->sd_write_idx = next_idx(idx);
}

// ─────────────────────────────────────────────
// Debug
// ─────────────────────────────────────────────

uint32_t audio_pipeline_get_overruns(audio_pipeline_t *pipeline) {
    return pipeline->overrun_count;
}

uint8_t audio_pipeline_get_pending_count(audio_pipeline_t *pipeline) {
    uint8_t count = 0;
    for (int i = 0; i < AUDIO_BLOCK_COUNT; i++) {
        if (pipeline->state[i] == BLOCK_READY ||
            pipeline->state[i] == BLOCK_PROCESSING ||
            pipeline->state[i] == BLOCK_WRITING) {
            count++;
        }
    }
    return count;
}
