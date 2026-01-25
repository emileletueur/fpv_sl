#ifndef MSC_DISK_H
#define MSC_DISK_H

#include <stdbool.h>
#include <stdint.h>


// Mark the start of an activity
bool tud_msc_busy(void);

// Check for activity timeout
void tud_msc_check_idle(void);

#endif // MSC_DISK_H