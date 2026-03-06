#include "msp_driver.h"

#include "debug_log.h"
#include "hardware/gpio.h"
#include "hardware/uart.h"
#include "pico/time.h"

#define MSP_READ_TIMEOUT_MS 10u

/* CRC8 DVB-S2, polynôme 0xD5.
   Couvre : flag + function_lo + function_hi + size_lo + size_hi + payload. */
static uint8_t crc8_dvb_s2(uint8_t crc, uint8_t a) {
    crc ^= a;
    for (int i = 0; i < 8; i++)
        crc = (crc & 0x80u) ? (uint8_t)((crc << 1u) ^ 0xD5u) : (uint8_t)(crc << 1u);
    return crc;
}

void msp_driver_init(msp_driver_t *drv, uint8_t uart_id, uint32_t baud_rate) {
    drv->uart = (uart_id == 0) ? uart0 : uart1;
    uart_init(drv->uart, baud_rate);
    gpio_set_function(PIN_MSP_UART_TX, GPIO_FUNC_UART);
    gpio_set_function(PIN_MSP_UART_RX, GPIO_FUNC_UART);
    LOGI("MSP UART%u init at %lu baud (TX=GP%u RX=GP%u).",
         uart_id, (unsigned long)baud_rate, PIN_MSP_UART_TX, PIN_MSP_UART_RX);
}

bool msp_send_request(msp_driver_t *drv, uint16_t function_id) {
    /* MSP v2 request frame : '$' 'X' '<' flag func_lo func_hi size_lo size_hi CRC */
    uint8_t flag    = 0u;
    uint8_t func_lo = (uint8_t)(function_id & 0xFFu);
    uint8_t func_hi = (uint8_t)(function_id >> 8u);
    uint8_t crc     = 0u;
    crc = crc8_dvb_s2(crc, flag);
    crc = crc8_dvb_s2(crc, func_lo);
    crc = crc8_dvb_s2(crc, func_hi);
    crc = crc8_dvb_s2(crc, 0u); /* size_lo */
    crc = crc8_dvb_s2(crc, 0u); /* size_hi */
    uint8_t frame[9] = {'$', 'X', '<', flag, func_lo, func_hi, 0u, 0u, crc};
    uart_write_blocking(drv->uart, frame, sizeof(frame));
    return true;
}

int8_t msp_read_response(msp_driver_t *drv, uint16_t expected_function,
                          uint8_t *buf, uint8_t *len) {
    typedef enum {
        S_DOLLAR, S_X, S_DIR,
        S_FLAG, S_FUNC_LO, S_FUNC_HI,
        S_SIZE_LO, S_SIZE_HI,
        S_PAYLOAD, S_CRC
    } state_t;

    state_t  state   = S_DOLLAR;
    uint8_t  crc     = 0u;
    uint8_t  func_lo = 0u;
    uint16_t size    = 0u;
    uint16_t idx     = 0u;
    *len             = 0u;

    uint32_t deadline = to_ms_since_boot(get_absolute_time()) + MSP_READ_TIMEOUT_MS;

    while (to_ms_since_boot(get_absolute_time()) < deadline) {
        if (!uart_is_readable(drv->uart))
            continue;

        uint8_t b = (uint8_t)uart_getc(drv->uart);

        switch (state) {
        case S_DOLLAR:
            if (b == '$') state = S_X;
            break;
        case S_X:
            state = (b == 'X') ? S_DIR : S_DOLLAR;
            break;
        case S_DIR:
            state = (b == '>') ? S_FLAG : S_DOLLAR;
            break;
        case S_FLAG:
            crc   = crc8_dvb_s2(crc, b);
            state = S_FUNC_LO;
            break;
        case S_FUNC_LO:
            func_lo = b;
            crc     = crc8_dvb_s2(crc, b);
            state   = S_FUNC_HI;
            break;
        case S_FUNC_HI: {
            uint16_t fn_id = (uint16_t)func_lo | ((uint16_t)b << 8u);
            crc = crc8_dvb_s2(crc, b);
            if (fn_id != expected_function) {
                LOGW("MSP: unexpected function %u (expected %u).", fn_id, expected_function);
                return -1;
            }
            state = S_SIZE_LO;
            break;
        }
        case S_SIZE_LO:
            size  = b;
            crc   = crc8_dvb_s2(crc, b);
            state = S_SIZE_HI;
            break;
        case S_SIZE_HI:
            size |= (uint16_t)b << 8u;
            crc   = crc8_dvb_s2(crc, b);
            state = (size > 0u) ? S_PAYLOAD : S_CRC;
            break;
        case S_PAYLOAD:
            if (idx < MSP_PAYLOAD_MAX)
                buf[idx] = b;
            idx++;
            crc = crc8_dvb_s2(crc, b);
            if (idx >= size)
                state = S_CRC;
            break;
        case S_CRC:
            *len = (uint8_t)(size < MSP_PAYLOAD_MAX ? size : MSP_PAYLOAD_MAX);
            if (b != crc) {
                LOGW("MSP: CRC error (got 0x%02x, expected 0x%02x).", b, crc);
                return -1;
            }
            return 0;
        }
    }

    LOGD("MSP: timeout waiting for function %u.", expected_function);
    return -1;
}
