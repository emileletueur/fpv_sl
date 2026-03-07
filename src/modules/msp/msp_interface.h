#pragma once

#include <stdbool.h>
#include <stdint.h>

/* Taille max d'un record de télémétrie packagé (timestamp + tous les blocs actifs).
   4 + 16 + 6 + 14 + 7 = 47 B max — 64 B de marge. */
#define TLM_RECORD_MAX 64u

typedef struct {
    uint8_t  uart_id;
    uint32_t baud_rate;
    uint8_t  enable_channel;    /* canal RC 1-based (ex. AUX1 = canal 5) */
    uint16_t channel_range_min; /* µs — début de la plage active */
    uint16_t channel_range_max; /* µs — fin de la plage active */
    uint16_t lipo_min_mv;       /* tension minimale LiPo en mV */
    uint8_t  telemetry_items;   /* bitmask tlm_item_flags_t — messages à poller en extra */
} msp_conf_t;

typedef int8_t (*msp_trigger_callback_t)(void);

/* Initialise le polling MSP.
 * Les callbacks suivent la même signature que gpio_interface. */
void initialize_msp_interface(const msp_conf_t *conf,
                               msp_trigger_callback_t on_enable,
                               msp_trigger_callback_t on_disable,
                               msp_trigger_callback_t on_record,
                               msp_trigger_callback_t on_disarm);

/* À appeler dans les boucles d'attente de Core 0.
 * Retourne true si un cycle de polling a été effectué (nouvelles données disponibles).
 * Retourne false si MSP non initialisé ou timer non échu. */
bool msp_poll_if_due(void);

/* Construit le record de télémétrie packagé pour le dernier cycle MSP.
 * Format dynamique : [uint32_t timestamp_ms][blocs actifs selon items].
 * Retourne le nombre de bytes écrits dans buf (≥ 4), ou 0 si pas de nouvelles données.
 * Doit être appelé immédiatement après msp_poll_if_due() == true. */
uint8_t msp_get_telemetry_record(uint8_t items, uint8_t *buf);

/* Retourne true si la tension LiPo est >= lipo_min_mv (après au moins un cycle MSP_ANALOG). */
bool msp_is_lipo_connected(void);
