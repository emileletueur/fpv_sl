#include "pico/stdlib.h"
#include <stdio.h>

#include "bsp/board_api.h"
#include "tusb.h"

int main() {
    
  board_init();
  stdio_init_all();

  while (true) {
    printf("Hello, world!\n");
    sleep_ms(1000);
  }

//   bool isDeviceEnumerated = false;
//   uint32_t start_time = to_ms_since_boot(get_absolute_time());

//   board_init();
//   tusb_init();

//   while (to_ms_since_boot(get_absolute_time()) - start_time <
//          USB_ENUM_TIMEOUT_MS) {
//     tud_task();
//     if (tud_mounted()) {
//       isDeviceEnumerated = true;
//       break;
//     }
//   }

//   if (isDeviceEnumerated) {
//     while (1) {
//       tud_task();
//     }
//   } else {
//     tud_disconnect();
//     sleep_ms(100);
//     boot_micropython();
//   }

//   return 0;
}
