#include "debug_cdc.h"
#include "tusb.h"
#include "tusb_config.h"
#include <stdarg.h>

#ifdef DEBUG_LOG_ENABLE
#warning "USB DEBUG LOG ACTIF"
#else
#warning "USB DEBUG LOG INACTIF"
#endif

void debug_cdc_vprintf(const char *fmt, va_list args)
{
    char buffer[256];
    int len = vsnprintf(buffer, sizeof(buffer), fmt, args);
    if (len > 0) {
        tud_cdc_write(buffer, len);
        tud_cdc_write_flush();
    }
}
