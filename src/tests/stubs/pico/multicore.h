#pragma once
/* Stub — remplace pico/multicore.h pour la compilation host (tests natifs) */
#include <stdint.h>

void     multicore_launch_core1(void (*fn)(void));
void     multicore_fifo_push_blocking(uint32_t data);
uint32_t multicore_fifo_pop_blocking(void);
