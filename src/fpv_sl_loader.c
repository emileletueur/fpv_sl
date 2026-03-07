#include "bsp/board_api.h"
#include "config/fpv_sl_config.h"
#include "debug_log.h"
// #include "modules/sdio/sd_spi_hw_config.h"
#include "fpv_sl_core.h"
#include "i2s_mic.h"
#include "modules/gpio/gpio_interface.h"
#include "modules/msp/msp_interface.h"
#include "modules/sdio/file_helper.h"
#include "modules/status_indicator/status_indicator.h"
#include "pico/multicore.h"
#include "status_indicator.h"
#include "tusb.h"
#include "usb/cdc_sim.h"
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

    // Init SD card (SPI + disk_initialize) — opération bloquante, avant tusb_init().
    tud_msc_request_mount();

    // Chargement config + récupération enregistrement non finalisé avant USB.
    if (read_conf_file() == 0) {
        recover_unfinalized_recording();
    }
    const fpv_sl_conf_t *conf = get_conf();

    // Init LED et démarrage USB.
    status_indicator_init();
    set_module_powered_status();

    tusb_init(BOARD_TUD_RHPORT, &dev_init);

    // Attente d'énumération USB (3 s).
    bool is_device_enumerated = false;
    uint32_t start_time = to_ms_since_boot(get_absolute_time());
    while (to_ms_since_boot(get_absolute_time()) - start_time < USB_ENUM_TIMEOUT_MS) {
        tud_task();
        if (tud_mounted()) {
            is_device_enumerated = true;
            break;
        }
    }

#ifndef FPV_SL_CDC_SIM
    // Mode MSC : expose la SD comme stockage USB, boucle infinie.
    if (is_device_enumerated) {
        LOGI("USB MSC mode.");
        f_mount(NULL, "0:", 0); // démonte FatFS — MSC prend le contrôle des secteurs SD.
        set_usb_msc_status();
        while (1) {
            tud_task();
            process_msc_activity();
        }
        /* unreachable */
    }
#endif

    // Mode enregistrement — atteint si : pas d'USB, timeout, ou FPV_SL_CDC_SIM actif.
    if (is_device_enumerated) {
        LOGI("USB CDC SIM — mode enregistrement avec CDC actif.");
        /* FatFS reste monté. cdc_poll() appellera fpv_sl_cdc_task() entre les blocs. */
    } else {
        LOGI("USB timeout — mode enregistrement.");
        tud_disconnect();
        sleep_ms(100);
    }

    if (conf->conf_is_loaded) {
        // Initialise le micro I2S.
        i2s_mic_t i2s_mic_conf = {
            .sample_rate = conf->sample_rate, .is_mono = conf->mono_record, .buffer_size = conf->buffer_size};
        init_i2s_mic(&i2s_mic_conf);

        // Initialise l'interface FC (GPIO ou MSP selon config).
        if (conf->use_uart_msp) {
            msp_conf_t msp_conf = {
                .uart_id           = conf->msp_uart_id,
                .baud_rate         = conf->msp_baud_rate,
                .enable_channel    = conf->msp_enable_channel,
                .channel_range_min = conf->msp_channel_range_min,
                .channel_range_max = conf->msp_channel_range_max,
                .lipo_min_mv       = conf->msp_lipo_min_mv,
                .telemetry_items   = conf->telemetry_items,
            };
            initialize_msp_interface(&msp_conf,
                fpv_sl_on_enable, fpv_sl_on_disable,
                fpv_sl_on_record, fpv_sl_on_disarm);
        } else {
            initialize_gpio_interface(fpv_sl_on_enable, fpv_sl_on_disable,
                fpv_sl_on_record, fpv_sl_on_disarm);
        }

        get_mode_from_config(conf);
        fpv_sl_process_mode();
    } else {
        LOGE("Config not loaded — cannot start recording.");
    }

    while (1) {
        tight_loop_contents();
    }
}
