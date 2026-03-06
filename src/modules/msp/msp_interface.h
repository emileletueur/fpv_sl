#pragma once

#include <stdbool.h>
#include <stdint.h>

typedef struct {
    uint8_t  uart_id;
    uint32_t baud_rate;
    uint8_t  enable_channel;    /* canal RC 1-based (ex. AUX1 = canal 5) */
    uint16_t channel_range_min; /* µs — début de la plage active */
    uint16_t channel_range_max; /* µs — fin de la plage active */
    uint16_t lipo_min_mv;       /* tension minimale LiPo en mV */
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
 * No-op si MSP non initialisé ou timer non échu. */
void msp_poll_if_due(void);

/* Retourne true si la tension LiPo est >= lipo_min_mv (après au moins un cycle MSP_ANALOG). */
bool msp_is_lipo_connected(void);
