#include "bsp/board_api.h"
#include "debug_cdc.h"
#include "modules/sdio/sd_spi_hw_config.h"
#include "modules/status_indicator/status_indicator.h"
#include "status_indicator.h"
#include "tusb.h"
#include "usb/msc_disk.h"
#include "usb/tusb_config.h"

#define USB_ENUM_TIMEOUT_MS 3000

static volatile bool was_busy = false;

void process_msc_activity(void) {
    // Check of MSC activity
    tud_msc_check_idle();
    bool is_busy = tud_msc_busy();

    if (is_busy && !was_busy) {
        // Idle to busy
        debug_cdc("fpv_sl->set_usb_msc_transer_status()\r\n");
        set_usb_msc_transer_status();
    } else if (!is_busy && was_busy) {
        // Busy to idle
        sleep_ms(50);
        debug_cdc("fpv_sl->set_usb_msc_status()\r\n");
        set_usb_msc_status();
    } else if (is_busy) {
        // still buzy
        // set_usb_msc_transer_status();
    }

    was_busy = is_busy;
}

int main() {
    // initialize the board and TinyUSB stack
    tusb_rhport_init_t dev_init = {.role = TUSB_ROLE_DEVICE, .speed = TUSB_SPEED_AUTO};
    board_init();
    // msc_sd_init();
    tusb_init(BOARD_TUD_RHPORT, &dev_init);

    // init status LED indicator
    status_indicator_init();
    set_module_powered_status();
    sleep_ms(500);

    // wait for device enumeration with timeout
    bool is_device_enumerated = false;
    uint32_t start_time = to_ms_since_boot(get_absolute_time());

    while (to_ms_since_boot(get_absolute_time()) - start_time < USB_ENUM_TIMEOUT_MS) {
        tud_task();
        if (tud_mounted()) {
            is_device_enumerated = true;
            break;
        }
    }

    if (is_device_enumerated) {
        debug_cdc("USB init OK\r\n");
        set_usb_msc_status();

        // main usb loop
        while (1) {
            tud_task();
            process_msc_activity();
        }
    } else {
        debug_cdc("USB timeout\r\n");
        // run main record loop
        tud_disconnect();
        sleep_ms(100);
        while (1) {
            // Votre code de record sans USB
            tight_loop_contents();
        }
    }
}
