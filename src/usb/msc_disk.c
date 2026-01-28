
#include "cdc/debug_cdc.h"
#include "ff.h"
#include "diskio.h"
#include "hw_config.h"
#include "tusb.h"
#include <pico/stdlib.h>

#define DISK_BLOCK_SIZE 512
#define PCNAME "0:"
#define MSC_LUN 0

struct {
    FATFS fs;
    bool sd_mounted;
    bool is_ejected;
    bool msc_is_busy;
} msc_state = {.fs = {0}, .sd_mounted = false, .is_ejected = false, .msc_is_busy = false};

static volatile bool storage_ready = false;
static volatile bool storage_init_in_progress = false;
static uint32_t cached_block_count = 0;

bool tud_msc_request_mount() {
    // DSTATUS ds = disk_initialize(MSC_LUN);
    // return (!(STA_NOINIT & ds) && !(STA_NODISK & ds));

    if (storage_ready || storage_init_in_progress) {
        return false;
    } else {
        storage_init_in_progress = true;

        DSTATUS ds = disk_initialize(MSC_LUN);
        if (!(ds & (STA_NOINIT | STA_NODISK))) {
            disk_ioctl(MSC_LUN, GET_SECTOR_COUNT, &cached_block_count);
            storage_ready = true;
        }

        storage_init_in_progress = false;
        return true;
    }
}

// Public function to check MSC activity
bool tud_msc_is_busy(void) {
    return msc_state.msc_is_busy;
}

// ------------------------------------------------------------------------------------------------------------------ //
// no-OS-FatFS-SD-SPI-RPi-Pico Implementation
// ------------------------------------------------------------------------------------------------------------------ //

// HW SPI Configuration
static spi_t spi_config = {
    .hw_inst = spi1,
    .miso_gpio = 12,
    .mosi_gpio = 11,
    .sck_gpio = 10,
    .baud_rate = 25000 * 1000 // 12.5 MHz
};

// SD Card Configuration
static sd_card_t sd_card = {.pcName = PCNAME,
                            .spi = &spi_config,
                            .ss_gpio = 13,
                            .use_card_detect = false,
                            .card_detect_gpio = -1,
                            .card_detected_true = -1};

size_t sd_get_num(void) {
    return 1;
}

sd_card_t *sd_get_by_num(size_t num) {
    if (num == 0) {
        return &sd_card;
    }
    return NULL;
}

size_t spi_get_num(void) {
    return 1;
}

spi_t *spi_get_by_num(size_t num) {
    if (num == 0) {
        return &spi_config;
    }
    return NULL;
}

// ------------------------------------------------------------------------------------------------------------------ //
// TinyUSB Implementation
// ------------------------------------------------------------------------------------------------------------------ //

// Inquiry request callback
void tud_msc_inquiry_cb(uint8_t lun, uint8_t vendor_id[8], uint8_t product_id[16], uint8_t product_rev[4]) {
    (void) lun;
    memcpy(vendor_id, "SGLab   ", 8);
    memcpy(product_id, "FPV-SL USB Flash", 16);
    memcpy(product_rev, "1.0", 4);
}

// LUN mount/ready request callback
bool tud_msc_test_unit_ready_cb(uint8_t lun) {
    (void) lun;
    // if (!sd_card.mounted) {
    //     DSTATUS ds = disk_initialize(lun);
    //     return (!(STA_NOINIT & ds) && !(STA_NODISK & ds));
    // }
    return storage_ready;
}

// LUN capacity request callback
void tud_msc_capacity_cb(uint8_t lun, uint32_t *block_count, uint16_t *block_size) {
    // if (!tud_msc_test_unit_ready_cb(lun)) {
    //     *block_count = 0;
    // } else {
    //     DRESULT dr = disk_ioctl(lun, GET_SECTOR_COUNT, block_count);
    //     if (RES_OK != dr)
    //         *block_count = 0;
    // }
    (void) lun;
    *block_count = cached_block_count;
    *block_size = 512;
}

// LUN START STOP UNIT request callback
bool tud_msc_start_stop_cb(uint8_t lun, uint8_t power_condition, bool start, bool load_eject) {
    (void) power_condition;
    if (load_eject) {
        if (start) {
            msc_state.is_ejected = false;
        } else {
            DRESULT dr = disk_ioctl(lun, CTRL_SYNC, 0);
            if (RES_OK != dr)
                return false;
            msc_state.is_ejected = true;
        }
    }
    return true;
}

// LUN is writable request callback
bool tud_msc_is_writable_cb(uint8_t lun) {
    if (msc_state.is_ejected)
        return false;
    DSTATUS ds = disk_status(lun);
    return !(STA_PROTECT & ds);
}

// LUN READ10 command callback
int32_t tud_msc_read10_cb(uint8_t lun, uint32_t lba, uint32_t offset, void *buffer, uint32_t bufsize) {
    (void) offset;
    assert(!offset);
    assert(!(bufsize % 512));

    if (msc_state.is_ejected)
        return -1;
    if (!tud_msc_test_unit_ready_cb(lun))
        return -1;

    msc_state.msc_is_busy = true;
    DRESULT dr = disk_read(lun, (BYTE *) buffer, lba, bufsize / 512);
    if (RES_OK != dr) {
        msc_state.msc_is_busy = false;
        return -1;
    }

    return (int32_t) bufsize;
}

void tud_msc_read10_complete_cb(uint8_t lun) {
    msc_state.msc_is_busy = false;
}

int32_t tud_msc_write10_cb(uint8_t lun, uint32_t lba, uint32_t offset, uint8_t *buffer, uint32_t bufsize) {
    (void) offset;
    assert(!offset);
    assert(!(bufsize % 512));

    if (msc_state.is_ejected)
        return -1;
    if (!storage_ready)
        return -1;

    msc_state.msc_is_busy = true;
    DRESULT dr = disk_write(lun, (BYTE *) buffer, lba, bufsize / 512);
    msc_state.msc_is_busy = false;
    if (RES_OK != dr) {
        return -1;
    }

    return bufsize;
}

// Callback invoked when received an SCSI command not in built-in list below
int32_t tud_msc_scsi_cb(uint8_t lun, uint8_t const scsi_cmd[16], void *buffer, uint16_t bufsize) {
    void const *response = NULL;
    int32_t resplen = 0;

    // most scsi handled is input
    bool in_xfer = true;

    switch (scsi_cmd[0]) {
    default:
        // Set Sense = Invalid Command Operation
        tud_msc_set_sense(lun, SCSI_SENSE_ILLEGAL_REQUEST, 0x20, 0x00);
        // negative means error -> tinyusb could stall and/or response with failed status
        resplen = -1;
        break;
    }

    if (resplen > bufsize)
        resplen = bufsize;

    if (response && (resplen > 0)) {
        if (in_xfer) {
            memcpy(buffer, response, (size_t) resplen);
        } else {
            ; // SCSI output
        }
    }
    return (int32_t) resplen;
}

// // Mark the start of an activity
// static inline void msc_set_busy(void) {
//     msc_state.msc_busy = true;
//     msc_state.last_activity_time = get_absolute_time();
// }

// // Check for activity timeout
// void tud_msc_check_idle(void) {
//     if (msc_state.msc_busy) {
//         // if after 200ms of inactivity, mark as idle
//         if (absolute_time_diff_us(msc_state.last_activity_time, get_absolute_time()) > 200000) {
//             msc_state.msc_busy = false;
//         }
//     }
// }

// bool tud_msc_is_writable_cb(uint8_t lun) {
//     (void) lun;
//     return false;
// }

// Invoked when device is mounted
// void tud_mount_cb(void) {
//     debug_cdc("tud_mount_cb");

//     if (!sd_init_driver()) {
//         debug_cdc("SD card init failed!");
//         msc_state.sd_mounted = false;
//         return;
//     }

//     // Monter la carte SD
//     FRESULT fr = f_mount(&msc_state.fs, "0:", 1);
//     if (fr == FR_OK) {
//         msc_state.sd_mounted = true;
//         debug_cdc("SD card mounted successfully");
//     } else {
//         msc_state.sd_mounted = false;
//         // debug_cdc("SD mount failed: %d", fr);
//         debug_cdc("SD mount failed !");
//     }
//     debug_cdc("Tud mounted !");
// }

// Invoked when device is unmounted
// void tud_umount_cb(void) {
//     debug_cdc("Tud unmounted !");
// }

// Invoked when usb bus is suspended
// remote_wakeup_en : if host allow us  to perform remote wakeup
// Within 7ms, device must draw an average of current less than 2.5 mA from bus
// void tud_suspend_cb(bool remote_wakeup_en) {
//     (void) remote_wakeup_en;
//     debug_cdc("Tud suspended !");
// }

// Invoked when usb bus is resumed
// void tud_resume_cb(void) {
//     debug_cdc("Tud resumed !");
// }

// Invoked when received SCSI_CMD_INQUIRY, v2 with full inquiry response
// uint32_t tud_msc_inquiry2_cb(uint8_t lun, scsi_inquiry_resp_t *inquiry_resp, uint32_t bufsize) {
//     (void) lun;
//     (void) bufsize;
//     const char vid[] = "TinyUSB";
//     const char pid[] = "Mass Storage";
//     const char rev[] = "1.0";

//     (void) strncpy((char *) inquiry_resp->vendor_id, vid, 8);
//     (void) strncpy((char *) inquiry_resp->product_id, pid, 16);
//     (void) strncpy((char *) inquiry_resp->product_rev, rev, 4);

//     return sizeof(scsi_inquiry_resp_t); // 36 bytes
// }
