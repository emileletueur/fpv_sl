#pragma once

void status_indicator_init(void);

// Module powered on -> fixed ON (debug: fixed) / white fixed (prod: RGB)
void set_module_powered_status(void);

// USB MSC connected -> slow blink (debug) / blue fixed (prod)
void set_usb_msc_status(void);

// USB MSC data transfer -> fast blink (debug) / blue blink (prod)
void set_usb_msc_transer_status(void);

// Ready to record -> double flash (debug) / green fixed (prod)
void set_module_record_ready_status(void);

// Recording active -> triple flash (debug) / green blink (prod)
void set_module_recording_status(void);

// Free disk alert -> quad flash (debug) / orange blink (prod)
void set_module_free_disk_alert_status(void);

// Free disk critical -> very fast blink (debug) / red blink (prod)
void set_module_free_disk_critical_status(void);
