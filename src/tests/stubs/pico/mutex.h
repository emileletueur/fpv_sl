#pragma once
/* Stub — remplace pico/mutex.h pour la compilation host (tests natifs) */

typedef struct { int _dummy; } mutex_t;

static inline void mutex_init(mutex_t *m)            { (void)m; }
static inline void mutex_enter_blocking(mutex_t *m)  { (void)m; }
static inline void mutex_exit(mutex_t *m)            { (void)m; }
