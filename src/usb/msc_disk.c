#include "cdc/debug_cdc.h"
#include "debug_cdc.h"
#include "ff.h"
#include "hw_config.h"
#include "tusb.h"
#include <stdbool.h>

#if CFG_TUD_MSC

struct {
    FATFS fs;
    bool sd_mounted;
    bool is_ejected;
    bool clear_cache_flag;
    bool msc_busy;
    uint32_t last_activity_time;
} msc_state = {.fs = {0},
               .sd_mounted = false,
               .is_ejected = false,
               .clear_cache_flag = false,
               .msc_busy = false,
               .last_activity_time = 0};

enum {
    DISK_BLOCK_NUM = 16, // 8KB is the smallest size that windows allow to mount
    DISK_BLOCK_SIZE = 512
};

// Some MCU doesn't have enough 8KB SRAM to store the whole disk
// We will use Flash as read-only disk with board that has
// CFG_EXAMPLE_MSC_READONLY defined

#define README_CONTENTS                                                                                                \
    "This is tinyusb's MassStorage Class demo.\r\n\r\n\
If you find any bugs or get any questions, feel free to file an\r\n\
issue at github.com/hathach/tinyusb"

static
#ifdef CFG_EXAMPLE_MSC_READONLY
    const
#endif
    uint8_t msc_disk[DISK_BLOCK_NUM][DISK_BLOCK_SIZE] = {
        //------------- Block0: Boot Sector -------------//
        {0xEB, 0x3C, 0x90, 0x4D, 0x53, 0x44, 0x4F, 0x53, 0x35, 0x2E, 0x30, 0x00, 0x02, 0x01, 0x01, 0x00, 0x01, 0x10,
         0x00, 0x10, 0x00, 0xF8, 0x01, 0x00, 0x01, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
         0x80, 0x00, 0x29, 0x34, 0x12, 0x00, 0x00, 'T', 'i', 'n', 'y', 'U', 'S', 'B', ' ', 'M', 'S', 'C', 0x46, 0x41,
         0x54, 0x31, 0x32, 0x20, 0x20, 0x20, 0x00, 0x00,

         // Zero up to 2 last bytes of FAT magic code
         0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
         0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
         0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
         0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
         0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
         0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
         0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
         0x00, 0x00,

         0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
         0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
         0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
         0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
         0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
         0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
         0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
         0x00, 0x00,

         0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
         0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
         0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
         0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
         0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
         0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
         0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
         0x00, 0x00,

         0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
         0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
         0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
         0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x55, 0xAA},

        //------------- Block1: FAT12 Table -------------//
        {
            0xF8, 0xFF, 0xFF, 0xFF, 0x0F // first 2 entries must be F8FF, third
                                         // entry is cluster end of readme file
        },

        //------------- Block2: Root Directory -------------//
        {
            // first entry is volume label
            'T', 'i', 'n', 'y', 'U', 'S', 'B', ' ', 'M', 'S', 'C', 0x08, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
            0x00, 0x00, 0x4F, 0x6D, 0x65, 0x43, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
            // second entry is readme file
            'R', 'E', 'A', 'D', 'M', 'E', ' ', ' ', 'T', 'X', 'T', 0x20, 0x00, 0xC6, 0x52, 0x6D, 0x65, 0x43, 0x65, 0x43,
            0x00, 0x00, 0x88, 0x6D, 0x65, 0x43, 0x02, 0x00, sizeof(README_CONTENTS) - 1, 0x00, 0x00,
            0x00 // readme's files size (4 Bytes)
        },

        //------------- Block3: Readme Content -------------//
        {README_CONTENTS}};

// ------------------------------------------------------------------------------------------------------------------ //
// no-OS-FatFS-SD-SPI-RPi-Pico Implementation
// ------------------------------------------------------------------------------------------------------------------ //
// Configuration SPI pour la carte SD
static spi_t spi_config = {
    .hw_inst = spi1, // ou spi1
    .miso_gpio = 12, // Adapter à vos pins
    .mosi_gpio = 11,
    .sck_gpio = 10,
    .baud_rate = 12500 * 1000 // 12.5 MHz
};

// Configuration de la carte SD
static sd_card_t sd_card = {.pcName = "0:",
                            .spi = &spi_config,
                            .ss_gpio = 13, // Chip Select
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

// Public function to check MSC activity
bool tud_msc_busy(void) {
    return msc_state.msc_busy;
}

// Mark the start of an activity
static inline void msc_set_busy(void) {
    msc_state.msc_busy = true;
    msc_state.last_activity_time = get_absolute_time();
}

// Check for activity timeout
void tud_msc_check_idle(void) {
    if (msc_state.msc_busy) {
        // if after 200ms of inactivity, mark as idle
        if (absolute_time_diff_us(msc_state.last_activity_time, get_absolute_time()) > 200000) {
            msc_state.msc_busy = false;
        }
    }
}

// Invoked when device is mounted
void tud_mount_cb(void) {
    debug_cdc("tud_mount_cb");

    if (!sd_init_driver()) {
        debug_cdc("SD card init failed!");
        msc_state.sd_mounted = false;
        return;
    }

    // Monter la carte SD
    FRESULT fr = f_mount(&msc_state.fs, "0:", 1);
    if (fr == FR_OK) {
        msc_state.sd_mounted = true;
        debug_cdc("SD card mounted successfully");
    } else {
        msc_state.sd_mounted = false;
        // debug_cdc("SD mount failed: %d", fr);
        debug_cdc("SD mount failed !");
    }
    debug_cdc("Tud mounted !");
}

// Invoked when device is unmounted
void tud_umount_cb(void) {
    debug_cdc("Tud unmounted !");
}

// Invoked when usb bus is suspended
// remote_wakeup_en : if host allow us  to perform remote wakeup
// Within 7ms, device must draw an average of current less than 2.5 mA from bus
void tud_suspend_cb(bool remote_wakeup_en) {
    (void) remote_wakeup_en;
    debug_cdc("Tud suspended !");
}

// Invoked when usb bus is resumed
void tud_resume_cb(void) {
    debug_cdc("Tud resumed !");
}

// Invoked when received SCSI_CMD_INQUIRY, v2 with full inquiry response
uint32_t tud_msc_inquiry2_cb(uint8_t lun, scsi_inquiry_resp_t *inquiry_resp, uint32_t bufsize) {
    (void) lun;
    (void) bufsize;
    const char vid[] = "TinyUSB";
    const char pid[] = "Mass Storage";
    const char rev[] = "1.0";

    (void) strncpy((char *) inquiry_resp->vendor_id, vid, 8);
    (void) strncpy((char *) inquiry_resp->product_id, pid, 16);
    (void) strncpy((char *) inquiry_resp->product_rev, rev, 4);

    return sizeof(scsi_inquiry_resp_t); // 36 bytes
}

#ifdef __cplusplus
extern "C" {
#endif

void tud_msc_inquiry_cb(uint8_t lun, uint8_t vendor_id[8], uint8_t product_id[16], uint8_t product_rev[4]) {
    (void) lun;

    memcpy(vendor_id, "SGLab   ", 8);
    memcpy(product_id, "FPV-SL USB Flash", 16);
    memcpy(product_rev, "1.0", 4);
}

#ifdef __cplusplus
}
#endif

// Invoked when received Test Unit Ready command.
bool tud_msc_test_unit_ready_cb(uint8_t lun) {
    (void) lun;

    // RAM disk is ready until msc_state.is_ejected
    if (msc_state.is_ejected) {
        // Additional Sense 3A-00 is NOT_FOUND
        return tud_msc_set_sense(lun, SCSI_SENSE_NOT_READY, 0x3a, 0x00);
    }

    // Force media changed right after write
    if (msc_state.clear_cache_flag) {
        msc_state.clear_cache_flag = false;
        debug_cdc("Signaling UNIT_ATTENTION (media changed)");
        // Code SCSI : Media Changed
        tud_msc_set_sense(lun, SCSI_SENSE_UNIT_ATTENTION, 0x28, 0x00);
        return false; // Pas prêt pour forcer un refresh
    }

    if (!msc_state.sd_mounted) {
        tud_msc_set_sense(lun, SCSI_SENSE_NOT_READY, 0x3a, 0x00);
        return false;
    }

    return true;
}

// Invoked when received SCSI_CMD_READ_CAPACITY_10 and
// SCSI_CMD_READ_FORMAT_CAPACITY to determine the disk size
void tud_msc_capacity_cb(uint8_t lun, uint32_t *block_count, uint16_t *block_size) {
    (void) lun;
    *block_count = DISK_BLOCK_NUM; // Valeur par défaut
    *block_size = DISK_BLOCK_SIZE;

    if (!msc_state.sd_mounted) {
        return;
    }

    DWORD fre_clust, fre_sect, tot_sect;
    FATFS *fs_ptr = &msc_state.fs; // Pointeur vers la structure
    FRESULT fr = f_getfree("0:", &fre_clust, &fs_ptr);
    if (fr != FR_OK) {
        debug_cdc("f_getfree failed!");
        return;
    }

    tot_sect = (msc_state.fs.n_fatent - 2) * msc_state.fs.csize;
    *block_count = tot_sect; // Mettre à jour avec la taille réelle
    char str_buffer[64];
    snprintf(str_buffer, sizeof(str_buffer), "Disk capacity: %lu sectors", *block_count);
    debug_cdc(str_buffer);
}

// Invoked when received Start Stop Unit command
bool tud_msc_start_stop_cb(uint8_t lun, uint8_t power_condition, bool start, bool load_eject) {
    (void) lun;
    (void) power_condition;

    if (load_eject) {
        if (start) {
            // load disk storage
            msc_state.is_ejected = false;
        } else {
            // unload disk storage
            msc_state.is_ejected = true;
        }
    }

    return true;
}

// Callback invoked when received READ10 command.
int32_t tud_msc_read10_cb(uint8_t lun, uint32_t lba, uint32_t offset, void *buffer, uint32_t bufsize) {
    (void) lun;
    msc_set_busy();

    if (!msc_state.sd_mounted) {
        return -1;
    }

    // Ouvrir un fichier (à adapter selon tes besoins)
    FIL file;
    FRESULT fr = f_open(&file, "0:/DUMMY.DAT", FA_READ);
    if (fr != FR_OK) {
        return -1;
    }

    // Lire les données
    UINT bytes_read;
    fr = f_lseek(&file, lba * DISK_BLOCK_SIZE + offset);
    if (fr != FR_OK) {
        f_close(&file);
        return -1;
    }

    fr = f_read(&file, buffer, bufsize, &bytes_read);
    f_close(&file);

    if (fr != FR_OK || bytes_read != bufsize) {
        return -1;
    }

    return (int32_t) bufsize;
}

bool tud_msc_is_writable_cb(uint8_t lun) {
    (void) lun;

#ifdef CFG_EXAMPLE_MSC_READONLY
    return false;
#else
    return true;
#endif
}

int32_t tud_msc_write10_cb(uint8_t lun, uint32_t lba, uint32_t offset, uint8_t *buffer, uint32_t bufsize) {
    (void) lun;
    msc_set_busy();

    // Vérifier que la carte SD est montée
    if (!msc_state.sd_mounted) {
        debug_cdc("Write10: SD card not mounted");
        return -1; // Échec
    }

    // Vérifier que le LBA est valide
    if (lba >= DISK_BLOCK_NUM) {
        debug_cdc("Write10: LBA out of range");
        return -1;
    }

    // Vérifier que la taille d'écriture est valide
    if (bufsize == 0 || (bufsize % DISK_BLOCK_SIZE) != 0) {
        debug_cdc("Write10: Invalid buffer size");
        return -1;
    }

    // Ouvrir le fichier (ou créer un fichier temporaire si nécessaire)
    FIL file;
    FRESULT fr = f_open(&file, "0:/DISK.DAT", FA_WRITE | FA_OPEN_ALWAYS);
    if (fr != FR_OK) {
        debug_cdc("Write10: Failed to open file");
        return -1;
    }

    // Se positionner à l'adresse correcte
    uint32_t file_offset = lba * DISK_BLOCK_SIZE + offset;
    fr = f_lseek(&file, file_offset);
    if (fr != FR_OK) {
        debug_cdc("Write10: Failed to seek");
        f_close(&file);
        return -1;
    }

    // Écrire les données
    UINT bytes_written;
    fr = f_write(&file, buffer, bufsize, &bytes_written);
    if (fr != FR_OK || bytes_written != bufsize) {
        debug_cdc("Write10: Failed to write data");
        f_close(&file);
        return -1;
    }

    // Fermer le fichier
    f_close(&file);

    // Invalider le cache (pour forcer le PC à rafraîchir)
    msc_state.clear_cache_flag = true;

    // debug_cdc("Write10: Success, wrote %lu bytes at LBA %lu", bufsize, lba);
    return (int32_t) bufsize;
}

// Callback invoked when received an SCSI command not in built-in list below
int32_t tud_msc_scsi_cb(uint8_t lun, uint8_t const scsi_cmd[16], void *buffer, uint16_t bufsize) {
    (void) lun;
    (void) scsi_cmd;
    (void) buffer;
    (void) bufsize;

    // currently no other commands is supported

    // Set Sense = Invalid Command Operation
    (void) tud_msc_set_sense(lun, SCSI_SENSE_ILLEGAL_REQUEST, 0x20, 0x00);

    return -1; // stall/failed command request;
}

#endif
