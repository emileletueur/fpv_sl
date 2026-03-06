/* Implémentations stub des symboles Pico SDK et des modules hardware-dépendants
   référencés au link par les unités de compilation compilées en mode host. */

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

/* Temps simulé — contrôlable depuis les tests via extern uint32_t test_time_ms. */
uint32_t test_time_ms = 0;

/* ── pico/multicore ─────────────────────────────────────────────────────── */

void     multicore_launch_core1(void (*fn)(void)) { (void)fn; }
void     multicore_fifo_push_blocking(uint32_t data) { (void)data; }
uint32_t multicore_fifo_pop_blocking(void) { return 0; }

/* ── i2s_mic ────────────────────────────────────────────────────────────── */

bool             is_data_ready(void)          { return false; }
volatile int32_t *get_active_buffer_ptr(void) { return NULL; }
uint32_t         get_current_data_count(void) { return 0; }
int8_t           i2s_mic_start(void)          { return 0; }
int8_t           i2s_mic_stop(void)           { return 0; }

/* ── sdio / file_helper ─────────────────────────────────────────────────── */

int8_t write_buffer(uint32_t *buff)                      { (void)buff; return 0; }
int8_t sync_wav_file(void)                               { return 0; }
int8_t create_wav_file(void)                             { return 0; }
int8_t finalize_wav_file(uint32_t rcd_duration)          { (void)rcd_duration; return 0; }
int8_t get_disk_usage_percent(uint8_t *out_pct)          { if (out_pct) *out_pct = 0; return 0; }

/* ── sdio / file_helper (supplémentaires) ───────────────────────────────── */

int8_t flush_audio_files(void) { return 0; }

/* ── msp_interface ───────────────────────────────────────────────────────── */

void msp_poll_if_due(void)        {}
bool msp_is_lipo_connected(void)  { return true; }

/* ── status_indicator ───────────────────────────────────────────────────── */

void status_indicator_init(void)              {}
void set_module_powered_status(void)          {}
void set_usb_msc_status(void)                 {}
void set_usb_msc_transer_status(void)         {}
void set_module_record_ready_status(void)     {}
void set_module_recording_status(void)        {}
void set_module_free_disk_alert_status(void)  {}
void set_module_free_disk_critical_status(void) {}
void set_module_flushing_status(void)         {}
