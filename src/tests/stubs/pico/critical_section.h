#pragma once
/* Stub — remplace pico/critical_section.h pour la compilation host (tests natifs) */

typedef struct { int _dummy; } critical_section_t;

static inline void critical_section_init(critical_section_t *cs)            { (void)cs; }
static inline void critical_section_enter_blocking(critical_section_t *cs)  { (void)cs; }
static inline void critical_section_exit(critical_section_t *cs)            { (void)cs; }
