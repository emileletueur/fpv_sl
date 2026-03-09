#pragma once
// #define USE_CUSTOM_BOARD_PINS

#include "audio_buffer.h"
#include "fpv_sl_config.h"

typedef enum {
    CLASSIC_TYPE,   // use ENABLE && RECORD trigger, dma will shutdown if ENABLE = false
    RCD_ONLY_TYPE,  // use RECORD only to trigger record, dma never sleep
    ALWAY_RCD_TYPE, // as soon as board is powered the recording is started
} execution_condition_t;

typedef struct {
    float alpha;   /* coefficient calculé depuis fc et fs */
    float last_x;  /* x[n-1] */
    float last_y;  /* y[n-1] */
} hp_filter_t;

typedef struct {
    float alpha;   /* coefficient calculé depuis fc et fs */
    float last_y;  /* y[n-1] */
} lp_filter_t;

/* Calcule l'alpha d'un filtre passe-haut IIR 1er ordre.
   alpha_hp = fs / (fs + 2π·fc) */
float compute_hp_alpha(uint16_t cutoff_hz, uint16_t sample_rate);

/* Calcule l'alpha d'un filtre passe-bas IIR 1er ordre.
   alpha_lp = 2π·fc / (fs + 2π·fc) */
float compute_lp_alpha(uint16_t cutoff_hz, uint16_t sample_rate);

/* Applique la chaîne de filtrage sur un échantillon brut INMP441 (32 bits).
   hp peut être NULL pour bypasser le filtre passe-haut.
   lp peut être NULL pour bypasser le filtre passe-bas.
   Le décalage d'alignement (>> 8) et le gain (0.8) sont toujours appliqués. */
int32_t process_sample(hp_filter_t *hp, lp_filter_t *lp, int32_t sample);

/* Pipeline audio partagé entre fpv_sl_core (traitement) et i2s_mic (DMA).
   Appeler fpv_sl_audio_pipeline_init() avant init_i2s_mic(). */
void              fpv_sl_audio_pipeline_init(void);
audio_pipeline_t *fpv_sl_get_audio_pipeline(void);

uint8_t get_mode_from_config(const fpv_sl_conf_t *fpv_sl_conf);
void fpv_sl_process_mode(void);
void fpv_sl_core0_loop(void);
void fpv_sl_core1_loop(void);

/* Triple-trigger ENABLE : vérifie / efface le flag de suppression des fichiers audio.
   fpv_sl_reset_enable_pulse_counter() est destiné aux tests et à la réinitialisation. */
bool fpv_sl_is_delete_requested(void);
void fpv_sl_clear_delete_request(void);
void fpv_sl_reset_enable_pulse_counter(void);

/* Callbacks GPIO — à passer à initialize_gpio_interface().
   Positionnent les flags internes de fpv_sl_core ; ne font aucun accès SD ni I2S. */
int8_t fpv_sl_on_enable(void);
int8_t fpv_sl_on_disable(void);
int8_t fpv_sl_on_record(void);
int8_t fpv_sl_on_disarm(void);
