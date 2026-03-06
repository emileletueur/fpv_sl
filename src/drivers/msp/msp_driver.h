#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "hardware/uart.h"

#ifndef PIN_MSP_UART_TX
#define PIN_MSP_UART_TX 4 /* UART1_TX — libre, pas de conflit */
#endif
#ifndef PIN_MSP_UART_RX
#define PIN_MSP_UART_RX 5 /* UART1_RX */
#endif

/* Taille maximale du payload reçu — le buffer passé à msp_read_response doit
   être au moins de cette taille. */
#define MSP_PAYLOAD_MAX 64u

typedef struct {
    uart_inst_t *uart;
} msp_driver_t;

/* Initialise le périphérique UART et les GPIOs associés. */
void msp_driver_init(msp_driver_t *drv, uint8_t uart_id, uint32_t baud_rate);

/* Envoie une requête MSP v2 (payload vide).
 * Frame : '$' 'X' '<' flag(0) func_lo func_hi size_lo(0) size_hi(0) CRC8-DVB-S2 */
bool msp_send_request(msp_driver_t *drv, uint16_t function_id);

/* Lit une réponse MSP v2.
 * Retourne 0 si OK, -1 en cas d'erreur (timeout, CRC, function_id inattendu).
 * *len est mis à jour avec le nombre d'octets reçus dans buf (≤ MSP_PAYLOAD_MAX). */
int8_t msp_read_response(msp_driver_t *drv, uint16_t expected_function,
                          uint8_t *buf, uint8_t *len);
