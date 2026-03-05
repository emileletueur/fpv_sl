#pragma once
/* Stub — remplace pico/time.h pour la compilation host (tests natifs) */
#include <stdint.h>

typedef uint64_t absolute_time_t;

/* Valeur retournée par to_ms_since_boot() — contrôlable depuis les tests. */
extern uint32_t test_time_ms;

static inline absolute_time_t get_absolute_time(void)              { return (absolute_time_t)0; }
static inline uint32_t        to_ms_since_boot(absolute_time_t t)  { (void)t; return test_time_ms; }
static inline void             sleep_ms(uint32_t ms)               { (void)ms; }
static inline void             tight_loop_contents(void)           {}
