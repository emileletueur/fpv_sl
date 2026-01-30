#include "debug_log.h"

/* backend sélectionné */
#include "debug_cdc.h"

void debug_log_vprintf(const char *fmt, va_list args)
{
    debug_cdc_vprintf(fmt, args);
}

void debug_log_printf(const char *fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    debug_log_vprintf(fmt, args);
    va_end(args);
}
