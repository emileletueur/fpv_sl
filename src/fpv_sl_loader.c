#include "bsp/board_api.h"
#include "config/fpv_sl_config.h"
#include "debug_log.h"
// #include "modules/sdio/sd_spi_hw_config.h"
#include "i2s_mic.h"
#include "modules/gpio/gpio_interface.h"
#include "modules/sdio/file_helper.h"
#include "modules/status_indicator/status_indicator.h"
#include "status_indicator.h"
#include "tusb.h"
#include "usb/msc_disk.h"
#include "usb/tusb_config.h"

#define USB_ENUM_TIMEOUT_MS 3000
#define USB_CDC_READY_MS 5000

bool is_config_loaded = false;

void process_msc_activity(void) {
    // Check of MSC activity
    bool is_busy = tud_msc_is_busy();

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
        LOGI("USB init OK.");
        set_usb_msc_status();

        // main usb loop
        while (1) {
            tud_task();
            process_msc_activity();
            if (tud_cdc_available()) {
                if (to_ms_since_boot(get_absolute_time()) - start_time > USB_CDC_READY_MS) {

                    // Load main config file
                    if (!is_config_loaded) {
                        read_conf_file();
                        const fpv_sl_conf_t *conf = get_conf();
                        is_config_loaded = conf->conf_is_loaded;
                        if (is_config_loaded) {

                            // Initialize I2S nems mic
                            i2s_mic_t i2s_mic_conf = {.sample_rate = conf->sample_rate,
                                                           .is_mono = conf->is_mono_rcd};
                            init_i2s_mic(&i2s_mic_conf);

                            // Initialize Flight Controller interface inputs
                            initialize_gpio_interface(i2s_mic_start, i2s_mic_stop);

                            // Setup the next record file

                            // Record main loop
                        }
                    }
                }
            }
        }
    } else {
        LOGI("USB timeout.");
        // run main record loop
        tud_disconnect();
        sleep_ms(100);
        while (1) {
            // Votre code de record sans USB
            // if (tud_cdc_available()) {
            //     LOGI("tud_cdc_available\r\n");
            //     if (!config_readed) {
            //         LOGI("Read config file !\r\n");
            //         read_conf_file();
            //         config_readed = true;
            //     }
            // }

            tight_loop_contents();
        }
    }
}
