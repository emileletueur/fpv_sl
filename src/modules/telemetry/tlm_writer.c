#include "tlm_writer.h"

#include "debug_log.h"
#include "ff.h"

#include <stdio.h>
#include <string.h>

#define TLM_TMP_PATH   "0:/t_mic_rcd.tlm"
#define TLM_FORMAT_VER 1u

/* f_sync toutes les TLM_SYNC_PERIOD écritures (~10 s à 30 Hz). */
#define TLM_SYNC_PERIOD 300u

static FIL      s_file;
static bool     s_open         = false;
static uint16_t s_records_since_sync = 0;

int8_t tlm_writer_open(uint8_t items, uint8_t protocol, uint8_t sample_rate_hz) {
    if (s_open) {
        LOGW("tlm_writer_open: already open, closing previous.");
        f_close(&s_file);
        s_open = false;
    }

    FRESULT fr = f_open(&s_file, TLM_TMP_PATH, FA_CREATE_ALWAYS | FA_WRITE);
    if (fr != FR_OK) {
        LOGE("tlm_writer_open: f_open failed (%d).", fr);
        return -1;
    }

    tlm_file_header_t hdr = {
        .magic           = {'F', 'P', 'V', 'T'},
        .version         = TLM_FORMAT_VER,
        .items           = items,
        .source_protocol = protocol,
        .sample_rate_hz  = sample_rate_hz,
    };
    UINT bw;
    fr = f_write(&s_file, &hdr, sizeof(hdr), &bw);
    if (fr != FR_OK || bw != sizeof(hdr)) {
        LOGE("tlm_writer_open: header write failed (%d).", fr);
        f_close(&s_file);
        return -1;
    }

    s_open = true;
    s_records_since_sync = 0;
    LOGI("TLM writer open (items=0x%02x, proto=%u, rate=%uHz).", items, protocol, sample_rate_hz);
    return 0;
}

int8_t tlm_writer_write(const uint8_t *buf, uint8_t len) {
    if (!s_open || len == 0)
        return 0;

    UINT bw;
    FRESULT fr = f_write(&s_file, buf, len, &bw);
    if (fr != FR_OK || bw != len) {
        LOGE("tlm_writer_write: f_write failed (%d).", fr);
        return -1;
    }

    if (++s_records_since_sync >= TLM_SYNC_PERIOD) {
        fr = f_sync(&s_file);
        if (fr != FR_OK)
            LOGW("tlm_writer_write: f_sync failed (%d).", fr);
        s_records_since_sync = 0;
    }
    return 0;
}

int8_t tlm_writer_close(uint16_t file_index, const char *folder, const char *prefix) {
    if (!s_open)
        return 0;

    FRESULT fr = f_close(&s_file);
    s_open = false;
    if (fr != FR_OK) {
        LOGE("tlm_writer_close: f_close failed (%d).", fr);
        return -1;
    }

    char dst[64];
    snprintf(dst, sizeof(dst), "0:/%s%s%u.tlm",
             folder ? folder : "",
             prefix ? prefix : "rec",
             file_index);

    fr = f_rename(TLM_TMP_PATH, dst);
    if (fr != FR_OK) {
        LOGE("tlm_writer_close: f_rename to %s failed (%d).", dst, fr);
        return -1;
    }

    LOGI("TLM file closed → %s.", dst);
    return 0;
}
