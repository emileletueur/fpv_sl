#include <stdio.h>
#include <stdlib.h>

#include "hardware/clocks.h"
#include "hardware/pio.h"
#include "pico/stdlib.h"
#include "status_indicator.h"
#include "ws2812.h"
// #ifdef PICO_DEFAULT_WS2812_PIN
// #define WS2812_PIN PICO_DEFAULT_WS2812_PIN
// #else
// // default to pin 2 if the board doesn't have a default WS2812 pin defined
#define WS2812_PIN 16
// #endif

// // Check the pin is compatible with the platform
// #if WS2812_PIN >= NUM_BANK0_GPIOS
// #error Attempting to use a pin>=32 on a platform that does not support it
// #endif

void status_indicator_init(void) { ws2812_init(WS2812_PIN); }

// Active USB MSC -> status indicator to fixed blue color
void set_usb_msc_status(void) { set_led(LED_BLUE, LED_MODE_FIXED); }

// Active USB MSC data transfer -> status indicator to blink blue color
void set_usb_msc_transer_status(void) { set_led(LED_BLUE, LED_MODE_BLINK); }

// Module powered on -> status indicator to fixed white color
void set_module_powered_status(void) { set_led(LED_WHITE, LED_MODE_FIXED); }

// Module ready to record -> status indicator to fixed green color
void set_module_record_ready_status(void) { set_led(LED_GREEN, LED_MODE_FIXED);  }

// Module recording -> status indicator to blink green color
void set_module_recording_status(void) { set_led(LED_GREEN, LED_MODE_BLINK); }

// Module free disk alert -> status indicator to blink orange color
void set_module_free_disk_alert_status(void) { set_led(LED_ORANGE, LED_MODE_BLINK); }

// Module free disk critical -> status indicator to blink red color
void set_module_free_disk_critical_status(void) { set_led(LED_RED, LED_MODE_BLINK); }