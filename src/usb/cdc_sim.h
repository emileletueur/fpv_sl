#pragma once

#ifdef FPV_SL_CDC_SIM

/* Appelé par fpv_sl_core.c via cdc_poll() à chaque bloc audio et dans les
   boucles idle. Parse tud_cdc_read() et dispatch les commandes 2 octets :
     e1 / e0  →  fpv_sl_on_enable / fpv_sl_on_disable
     r1 / r0  →  fpv_sl_on_record / fpv_sl_on_disarm */
void fpv_sl_cdc_task(void);

#endif /* FPV_SL_CDC_SIM */
