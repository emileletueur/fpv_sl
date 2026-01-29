#include "debug_cdc.h"
#include "tusb.h"
#include "tusb_config.h"
#include <stdarg.h>

void debug_cdc(const char *msg) {
    // if (tud_cdc_available()) {
    tud_cdc_write_str(msg);
    tud_cdc_write_flush();
    // }
}

void debug_cdc_fmt(const char *fmt, ...) {
    char buffer[256];
    va_list args;
    va_start(args, fmt);
    int len = vsnprintf(buffer, sizeof(buffer), fmt, args);
    va_end(args);
    if (len > 0) {
        tud_cdc_write(buffer, len);
        tud_cdc_write_flush();
    }
}
