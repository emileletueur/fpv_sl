#include "status_indicator.h"

#ifdef USE_PICO_ONBOARD_LED

// ─────────────────────────────────────────────────────────────────────────────
// Debug mode: Pi Pico classique — LED onboard GP25
// Patterns de clignotement pour remplacer les couleurs RGB
//
// Scénario:
//   Fixe allumée    → alimenté / boot
//   Blink lent      → USB MSC connecté      (500ms)
//   Blink rapide    → Transfert USB         (100ms)
//   Double flash    → Prêt à enregistrer
//   Triple flash    → Enregistrement actif  ← état le plus important en vol
//   Quadruple flash → Alerte disque (>80%)
//   Blink très rapide → Disque critique (>95%)
// ─────────────────────────────────────────────────────────────────────────────

#include "hardware/gpio.h"
#include "pico/time.h"

#ifndef PICO_DEFAULT_LED_PIN
#define STATUS_LED_PIN 25
#else
#define STATUS_LED_PIN PICO_DEFAULT_LED_PIN
#endif

#define TICK_MS 10  // Résolution du timer (10ms = 100Hz)

// Chaque pattern est une suite de durées ON/OFF alternées en millisecondes.
// step 0 = ON, step 1 = OFF, step 2 = ON, ...
static const uint16_t pat_fixed_on[]     = {5000, 10};
static const uint16_t pat_slow_blink[]   = {500, 500};
static const uint16_t pat_fast_blink[]   = {100, 100};
static const uint16_t pat_double_flash[] = {100, 150, 100, 900};
static const uint16_t pat_triple_flash[] = {100, 150, 100, 150, 100, 900};
static const uint16_t pat_quad_flash[]   = {50, 60, 50, 60, 50, 60, 50, 800};
static const uint16_t pat_sos[]          = {50, 50};

typedef struct {
    const uint16_t *steps;
    uint8_t         count;
} led_pattern_t;

typedef enum {
    PAT_FIXED_ON = 0,
    PAT_SLOW_BLINK,
    PAT_FAST_BLINK,
    PAT_DOUBLE_FLASH,
    PAT_TRIPLE_FLASH,
    PAT_QUAD_FLASH,
    PAT_SOS,
} pattern_id_t;

static const led_pattern_t patterns[] = {
    [PAT_FIXED_ON]     = {pat_fixed_on,     2},
    [PAT_SLOW_BLINK]   = {pat_slow_blink,   2},
    [PAT_FAST_BLINK]   = {pat_fast_blink,   2},
    [PAT_DOUBLE_FLASH] = {pat_double_flash,  4},
    [PAT_TRIPLE_FLASH] = {pat_triple_flash,  6},
    [PAT_QUAD_FLASH]   = {pat_quad_flash,    8},
    [PAT_SOS]          = {pat_sos,           2},
};

static volatile pattern_id_t current_pattern = PAT_FIXED_ON;
static volatile uint8_t      step_idx        = 0;
static volatile uint16_t     tick_acc        = 0;

static struct repeating_timer led_timer;

static bool led_timer_cb(struct repeating_timer *t) {
    const led_pattern_t *pat = &patterns[current_pattern];

    tick_acc += TICK_MS;
    if (tick_acc >= pat->steps[step_idx]) {
        tick_acc = 0;
        step_idx = (step_idx + 1) % pat->count;
        // step pair = ON, step impair = OFF
        gpio_put(STATUS_LED_PIN, (step_idx % 2 == 0) ? 1 : 0);
    }
    return true;
}

static void set_pattern(pattern_id_t pat) {
    current_pattern = pat;
    step_idx        = 0;
    tick_acc        = 0;
    gpio_put(STATUS_LED_PIN, 1);  // step 0 toujours ON
}

void status_indicator_init(void) {
    gpio_init(STATUS_LED_PIN);
    gpio_set_dir(STATUS_LED_PIN, GPIO_OUT);
    gpio_put(STATUS_LED_PIN, 0);
    add_repeating_timer_ms(-TICK_MS, led_timer_cb, NULL, &led_timer);
}

void set_module_powered_status(void)            { set_pattern(PAT_FIXED_ON);     }
void set_usb_msc_status(void)                   { set_pattern(PAT_SLOW_BLINK);   }
void set_usb_msc_transer_status(void)           { set_pattern(PAT_FAST_BLINK);   }
void set_module_record_ready_status(void)       { set_pattern(PAT_DOUBLE_FLASH); }
void set_module_recording_status(void)          { set_pattern(PAT_TRIPLE_FLASH); }
void set_module_free_disk_alert_status(void)    { set_pattern(PAT_QUAD_FLASH);   }
void set_module_free_disk_critical_status(void) { set_pattern(PAT_SOS);          }

#else

// ─────────────────────────────────────────────────────────────────────────────
// Production: LED RGB WS2812
// ─────────────────────────────────────────────────────────────────────────────

#include "ws2812.h"

#define WS2812_PIN 16

void status_indicator_init(void)                { ws2812_init(WS2812_PIN);                    }
void set_module_powered_status(void)            { set_led(LED_WHITE,  LED_MODE_FIXED);         }
void set_usb_msc_status(void)                   { set_led(LED_BLUE,   LED_MODE_FIXED);         }
void set_usb_msc_transer_status(void)           { set_led(LED_BLUE,   LED_MODE_BLINK);         }
void set_module_record_ready_status(void)       { set_led(LED_GREEN,  LED_MODE_FIXED);         }
void set_module_recording_status(void)          { set_led(LED_GREEN,  LED_MODE_BLINK);         }
void set_module_free_disk_alert_status(void)    { set_led(LED_ORANGE, LED_MODE_BLINK);         }
void set_module_free_disk_critical_status(void) { set_led(LED_RED,    LED_MODE_BLINK);         }

#endif
