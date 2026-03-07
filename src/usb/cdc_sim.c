#include "cdc_sim.h"

#ifdef FPV_SL_CDC_SIM

#include "tusb.h"

/* Callbacks définis dans fpv_sl_core.c — pas de dépendance circulaire. */
extern int8_t fpv_sl_on_enable(void);
extern int8_t fpv_sl_on_disable(void);
extern int8_t fpv_sl_on_record(void);
extern int8_t fpv_sl_on_disarm(void);

void fpv_sl_cdc_task(void) {
    tud_task();
    if (!tud_cdc_available())
        return;
    char buf[8];
    uint32_t n = tud_cdc_read(buf, sizeof(buf));
    for (uint32_t i = 0; i + 1 < n; i++) {
        char cmd = buf[i], val = buf[i + 1];
        if      (cmd == 'e' && val == '1') { fpv_sl_on_enable();  i++; }
        else if (cmd == 'e' && val == '0') { fpv_sl_on_disable(); i++; }
        else if (cmd == 'r' && val == '1') { fpv_sl_on_record();  i++; }
        else if (cmd == 'r' && val == '0') { fpv_sl_on_disarm();  i++; }
    }
}

#endif /* FPV_SL_CDC_SIM */
