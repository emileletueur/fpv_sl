#pragma once

#include <stdint.h>

// LED colors
typedef enum { LED_OFF, LED_BLUE, LED_GREEN, LED_RED, LED_ORANGE, LED_WHITE } led_color_t;

// LED modes
typedef enum { LED_MODE_FIXED, LED_MODE_BLINK } led_mode_t;

void ws2812_init(uint8_t pin);
void set_led(led_color_t color, led_mode_t mode);
void off(void);
