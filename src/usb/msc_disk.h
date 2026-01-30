#ifndef MSC_DISK_H
#define MSC_DISK_H

#ifndef PIN_SD_SPI_SCK
#define PIN_SD_SPI_SCK  10
#endif

#ifndef PIN_SD_SPI_MOSI
#define PIN_SD_SPI_MOSI 11
#endif

#ifndef PIN_SD_SPI_MISO
#define PIN_SD_SPI_MISO 12
#endif

#ifndef PIN_SD_SPI_CS
#define PIN_SD_SPI_CS   13
#endif

#include <stdbool.h>
#include <stdint.h>

// Request for mount SD
bool tud_msc_request_mount(void);
// Mark the start of an activity
bool tud_msc_is_busy(void);

#endif // MSC_DISK_H
