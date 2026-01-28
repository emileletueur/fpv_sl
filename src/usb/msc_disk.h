#ifndef MSC_DISK_H
#define MSC_DISK_H

#include <stdbool.h>
#include <stdint.h>

// Request for mount SD
bool tud_msc_request_mount(void);
// Mark the start of an activity
bool tud_msc_is_busy(void);

#endif // MSC_DISK_H
