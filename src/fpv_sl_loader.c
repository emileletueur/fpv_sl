#include "bsp/board_api.h"
#include "config/fpv_sl_config.h"
#include "debug_log.h"
// #include "modules/sdio/sd_spi_hw_config.h"
#include "fpv_sl_core.h"
#include "i2s_mic.h"
#include "modules/gpio/gpio_interface.h"
#include "modules/sdio/file_helper.h"
#include "modules/status_indicator/status_indicator.h"
#include "pico/multicore.h"
#include "status_indicator.h"
#include "tusb.h"
#include "usb/msc_disk.h"
#include "usb/tusb_config.h"

#define USB_ENUM_TIMEOUT_MS 3000

void process_msc_activity(void) {
    if (tud_msc_is_busy()) {
        set_usb_msc_transer_status();
    } else {
        set_usb_msc_status();
    }
}

// Invoked when device is unmounted
void tud_umount_cb(void) {
    LOGI("Ejected.");
}

int main() {
    // initialize the board and TinyUSB stack
    tusb_rhport_init_t dev_init = {.role = TUSB_ROLE_DEVICE, .speed = TUSB_SPEED_AUTO};

    board_init_after_tusb();

    while (tud_msc_request_mount()) {
        ;
    }

    tusb_init(BOARD_TUD_RHPORT, &dev_init);

    // init status LED indicator
    status_indicator_init();
    set_module_powered_status();
    sleep_ms(500);

    // Load config and recover any unfinalized recording (before USB/non-USB split)
    // so that recovered files are visible via MSC if USB is detected.
    if (read_conf_file() == 0) {
        recover_unfinalized_recording();
    }
    const fpv_sl_conf_t *conf = get_conf();

    // wait for USB enumeration with timeout
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
        LOGI("USB init OK.");
        // Unmount FatFS before MSC takes over SD card sector access.
        f_mount(NULL, "0:", 0);
        set_usb_msc_status();
        while (1) {
            tud_task();
            process_msc_activity();
        }
    } else {
        LOGI("USB timeout — recording mode.");
        tud_disconnect();
        sleep_ms(100);

        if (conf->conf_is_loaded) {
            // Initialize I2S MEMS mic
            i2s_mic_t i2s_mic_conf = {
                .sample_rate = conf->sample_rate, .is_mono = conf->is_mono_rcd, .buffer_size = 2048};
            init_i2s_mic(&i2s_mic_conf);

            // Initialize Flight Controller GPIO interface
            initialize_gpio_interface(i2s_mic_start, i2s_mic_stop);

            // Determine execution mode and start recording
            get_mode_from_config(conf);
            fpv_sl_process_mode();
        } else {
            LOGE("Config not loaded — cannot start recording.");
        }

        while (1) {
            tight_loop_contents();
        }
    }
}
