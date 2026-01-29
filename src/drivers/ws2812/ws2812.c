/**
 * Copyright (c) 2020 Raspberry Pi (Trading) Ltd.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "ws2812.h"
#include "../../usb/cdc/debug_cdc.h"
#include "hardware/pio.h"
#include "pico/time.h"
#include "ws2812.pio.h"
#include <stdio.h>
#include <stdlib.h>

#define WS2812_SM 0
#define WS2812_FREQ 800000

// Colors
static const uint32_t COLOR_VALUES[] = {[LED_OFF] = 0x000000, [LED_BLUE] = 0x0000FF,   [LED_GREEN] = 0x00FF00,
                                        [LED_RED] = 0xFF0000, [LED_ORANGE] = 0xFF6500, [LED_WHITE] = 0xFFFFFF};

static const char *COLOR_NAMES[] = {
    [LED_OFF] = "OFF", [LED_BLUE] = "BLUE", [LED_GREEN] = "GREEN", [LED_RED] = "RED", [LED_ORANGE] = "ORANGE"};

// Actual state
static struct {
    led_color_t color;
    led_mode_t mode;
    uint32_t last_toggle;
    bool blink_state;
} led_state = {.color = LED_OFF, .mode = LED_MODE_FIXED, .last_toggle = 0, .blink_state = false};

static PIO ws_pio;
static uint ws_sm;
static struct repeating_timer blink_timer;

static inline void put_pixel(uint32_t pixel_grb) {
    pio_sm_put_blocking(ws_pio, ws_sm, pixel_grb << 8u);
}

static inline uint32_t urgb_u32(uint8_t r, uint8_t g, uint8_t b) {
    return ((uint32_t) (r) << 8) | ((uint32_t) (g) << 16) | (uint32_t) (b);
}

static bool blink_timer_callback(struct repeating_timer *t) {
    if (led_state.mode == LED_MODE_BLINK) {
        led_state.blink_state = !led_state.blink_state;

        uint32_t color = led_state.blink_state ? COLOR_VALUES[led_state.color] : COLOR_VALUES[LED_OFF];

        put_pixel(urgb_u32((COLOR_VALUES[color] >> 16) & 0xFF, (COLOR_VALUES[color] >> 8) & 0xFF,
                           COLOR_VALUES[color] & 0xFF));
    }
    return true;
}

void ws2812_init(uint8_t pin) {
    ws_pio = pio0;
    ws_sm = WS2812_SM;
    uint offset = pio_add_program(ws_pio, &ws2812_program);
    ws2812_program_init(ws_pio, ws_sm, offset, pin, WS2812_FREQ, false);
    add_repeating_timer_ms(250, blink_timer_callback, NULL, &blink_timer);
}

void set_led(led_color_t color, led_mode_t mode) {
    led_state.color = color;
    led_state.mode = mode;
    char buffer[64];

    if (mode == LED_MODE_FIXED) {
        put_pixel(urgb_u32((COLOR_VALUES[color] >> 16) & 0xFF, // R
                           (COLOR_VALUES[color] >> 8) & 0xFF,  // G
                           COLOR_VALUES[color] & 0xFF          // B
                           ));
        // char buffer[64];
        // snprintf(buffer, sizeof(buffer), "[STATUS] LED set to color %s (fixed)\r\n", COLOR_NAMES[color]);
        // debug_cdc(buffer);
    } else {
        led_state.blink_state = false;
        // snprintf(buffer, sizeof(buffer), "[STATUS] LED set to color %s  (blink)\r\n", COLOR_NAMES[color]);
        // debug_cdc(buffer);
    }
}

void off(void) {
    put_pixel(0);
}
