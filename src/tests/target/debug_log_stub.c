#include <stdio.h>
#include <stdarg.h>

void debug_log_vprintf(const char *fmt, va_list args) {
    vprintf(fmt, args);
}

void debug_log_printf(const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);
    vprintf(fmt, args);
    va_end(args);
}
