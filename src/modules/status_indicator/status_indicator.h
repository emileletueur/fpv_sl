#pragma once

#include "hardware/pio.h"
#include <stdint.h>

void status_indicator_init(void);

// Active USB MSC -> status indicator to fixed blue color
void set_usb_msc_status(void);

// Active USB MSC data transfer -> status indicator to blink blue color
void set_usb_msc_transer_status(void);

// Module powered on -> status indicator to blink white color
void set_module_powered_status(void);

// Module ready to record -> status indicator to fixed green color
void set_module_record_ready_status(void);

// Module recording -> status indicator to blink green color
void set_module_recording_status(void);

// Module free disk alert -> status indicator to blink orange color
void set_module_free_disk_alert_status(void);

// Module free disk critical -> status indicator to blink red color
void set_module_free_disk_critical_status(void);