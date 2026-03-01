/* Implémentations stub des symboles Pico SDK et des modules hardware-dépendants
   référencés au link par les unités de compilation compilées en mode host. */

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

/* ── pico/multicore ─────────────────────────────────────────────────────── */

void     multicore_launch_core1(void (*fn)(void)) { (void)fn; }
void     multicore_fifo_push_blocking(uint32_t data) { (void)data; }
uint32_t multicore_fifo_pop_blocking(void) { return 0; }

/* ── i2s_mic ────────────────────────────────────────────────────────────── */

bool             is_data_ready(void)          { return false; }
volatile int32_t *get_active_buffer_ptr(void) { return NULL; }
uint32_t         get_current_data_count(void) { return 0; }

/* ── sdio / file_helper ─────────────────────────────────────────────────── */

int8_t write_buffer(uint32_t *buff) { (void)buff; return 0; }
