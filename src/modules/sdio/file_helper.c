#include "file_helper.h"
#include "ff.h"
#include "../../usb/cdc/debug_cdc.h"

uint8_t read_file(FIL *file_p, char *file_name, BYTE mode) {
    FRESULT fr;
    fr = f_open(file_p, file_name, mode);
    if (fr != FR_OK) {
        debug_cdc("Config file not found, using defaults\r\n");
        return false;
    }
    return 0;
}
uint8_t read_conf_file(void) {
    return true;
}
uint8_t create_wav_file();
uint8_t append_wav_header();
uint8_t get_file_name();
uint8_t list_wav_files(void);
