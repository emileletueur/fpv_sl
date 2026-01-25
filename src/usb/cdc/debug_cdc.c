#include "tusb_config.h"
#include "tusb.h"
#include "debug_cdc.h"

void debug_cdc(const char *msg)
{
    if (tud_cdc_connected())
    {
        tud_cdc_write_str(msg);
        tud_cdc_write_flush();
    }
}