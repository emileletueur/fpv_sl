#include "debug_cdc.h"
#include "tusb.h"
#include "tusb_config.h"


void debug_cdc(const char *msg) {
    if (tud_cdc_available()) {
        tud_cdc_write_str(msg);
        tud_cdc_write_flush();
    }
}
