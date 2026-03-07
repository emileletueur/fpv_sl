#include "msp_interface.h"

#include "debug_log.h"
#include "fpv_sl_config.h"
#include "msp_driver.h"
#include "pico/time.h"

#include <string.h>

/* Function IDs MSP v2 — identiques aux cmd v1 pour les messages standard Betaflight */
#define MSP_CMD_STATUS   ((uint16_t)101u)
#define MSP_CMD_RC       ((uint16_t)105u)
#define MSP_CMD_RAW_GPS  ((uint16_t)106u)
#define MSP_CMD_ATTITUDE ((uint16_t)108u)
#define MSP_CMD_ANALOG   ((uint16_t)110u)

/* ── État interne ──────────────────────────────────────────────────────────── */

static msp_driver_t           s_drv;
static msp_conf_t             s_conf;
static msp_trigger_callback_t s_on_enable  = NULL;
static msp_trigger_callback_t s_on_disable = NULL;
static msp_trigger_callback_t s_on_record  = NULL;
static msp_trigger_callback_t s_on_disarm  = NULL;

static volatile bool     s_initialized   = false;
static volatile bool     s_msp_due       = false;
static repeating_timer_t s_timer;

static bool     s_armed         = false;
static bool     s_enable_active = false;

/* ── Dernières valeurs MSP — pour la télémétrie ───────────────────────────── */
static uint16_t s_rc_channels[8]  = {0}; /* CH1-CH8, µs */
static int16_t  s_attitude[3]     = {0}; /* roll, pitch (deci-deg), yaw (deg) */
static uint8_t  s_gps_fix         = 0;
static uint8_t  s_gps_sats        = 0;
static int32_t  s_gps_lat         = 0;   /* degE7 */
static int32_t  s_gps_lon         = 0;   /* degE7 */
static uint16_t s_gps_alt         = 0;   /* mètres */
static uint16_t s_gps_speed       = 0;   /* cm/s */
static uint16_t s_vbat_mv         = 0;   /* mV */
static uint16_t s_analog_mah      = 0;
static uint16_t s_analog_rssi     = 0;   /* 0-1023 */
static int16_t  s_analog_current  = 0;   /* centi-ampères */
static bool     s_tlm_ready       = false;

/* ── Timer callback (~30 Hz) ───────────────────────────────────────────────── */

static bool msp_timer_cb(repeating_timer_t *t) {
    (void)t;
    s_msp_due = true;
    return true;
}

/* ── Cycle de polling ──────────────────────────────────────────────────────── */

static void msp_poll_cycle(void) {
    uint8_t buf[MSP_PAYLOAD_MAX];
    uint8_t len = 0;

    /* ── MSP_STATUS — détection ARM ── */
    msp_send_request(&s_drv, MSP_CMD_STATUS);
    if (msp_read_response(&s_drv, MSP_CMD_STATUS, buf, &len) == 0 && len >= 10u) {
        uint32_t flags = 0;
        memcpy(&flags, buf + 6, sizeof(flags));
        bool armed = (flags & 0x01u) != 0u;
        if (armed && !s_armed) {
            LOGI("MSP: ARM détecté.");
            if (s_on_record)
                s_on_record();
        } else if (!armed && s_armed) {
            LOGI("MSP: DISARM détecté.");
            if (s_on_disarm)
                s_on_disarm();
        }
        s_armed = armed;
    }

    /* ── MSP_RC — canal ENABLE + stockage CH1-8 ── */
    msp_send_request(&s_drv, MSP_CMD_RC);
    if (msp_read_response(&s_drv, MSP_CMD_RC, buf, &len) == 0) {
        /* Stocker tous les canaux disponibles (max 8). */
        uint8_t max_ch = len / 2u;
        for (uint8_t i = 0; i < 8u && i < max_ch; i++)
            memcpy(&s_rc_channels[i], buf + i * 2u, sizeof(uint16_t));

        uint8_t ch_idx  = s_conf.enable_channel - 1u;
        uint16_t val    = (ch_idx < max_ch) ? s_rc_channels[ch_idx] : 0u;
        bool active = (val >= s_conf.channel_range_min && val <= s_conf.channel_range_max);
        if (active && !s_enable_active) {
            LOGI("MSP: ENABLE canal %u actif (val=%u µs).", s_conf.enable_channel, val);
            if (s_on_enable)
                s_on_enable();
        } else if (!active && s_enable_active) {
            LOGI("MSP: ENABLE canal %u inactif (val=%u µs).", s_conf.enable_channel, val);
            if (s_on_disable)
                s_on_disable();
        }
        s_enable_active = active;
    }

    /* ── MSP_ATTITUDE — roll/pitch/yaw (si demandé) ── */
    if (s_conf.telemetry_items & TLM_ATTITUDE) {
        msp_send_request(&s_drv, MSP_CMD_ATTITUDE);
        if (msp_read_response(&s_drv, MSP_CMD_ATTITUDE, buf, &len) == 0 && len >= 6u)
            memcpy(s_attitude, buf, sizeof(s_attitude));
    }

    /* ── MSP_RAW_GPS — fix/sats/lat/lon/alt/speed (si demandé) ── */
    if (s_conf.telemetry_items & TLM_GPS) {
        msp_send_request(&s_drv, MSP_CMD_RAW_GPS);
        if (msp_read_response(&s_drv, MSP_CMD_RAW_GPS, buf, &len) == 0 && len >= 14u) {
            s_gps_fix   = buf[0];
            s_gps_sats  = buf[1];
            memcpy(&s_gps_lat,   buf + 2,  sizeof(s_gps_lat));
            memcpy(&s_gps_lon,   buf + 6,  sizeof(s_gps_lon));
            memcpy(&s_gps_alt,   buf + 10, sizeof(s_gps_alt));
            memcpy(&s_gps_speed, buf + 12, sizeof(s_gps_speed));
        }
    }

    /* ── MSP_ANALOG — tension LiPo + champs télémétrie ── */
    msp_send_request(&s_drv, MSP_CMD_ANALOG);
    if (msp_read_response(&s_drv, MSP_CMD_ANALOG, buf, &len) == 0 && len >= 1u) {
        s_vbat_mv = (uint16_t)buf[0] * 100u;
        if (len >= 7u) {
            memcpy(&s_analog_mah,     buf + 1, sizeof(s_analog_mah));
            memcpy(&s_analog_rssi,    buf + 3, sizeof(s_analog_rssi));
            memcpy(&s_analog_current, buf + 5, sizeof(s_analog_current));
        }
        LOGD("MSP: vbat=%u mV.", s_vbat_mv);
    }

    s_tlm_ready = true;
}

/* ── API publique ──────────────────────────────────────────────────────────── */

void initialize_msp_interface(const msp_conf_t *conf,
                               msp_trigger_callback_t on_enable,
                               msp_trigger_callback_t on_disable,
                               msp_trigger_callback_t on_record,
                               msp_trigger_callback_t on_disarm) {
    s_conf       = *conf;
    s_on_enable  = on_enable;
    s_on_disable = on_disable;
    s_on_record  = on_record;
    s_on_disarm  = on_disarm;

    msp_driver_init(&s_drv, conf->uart_id, conf->baud_rate);

    /* Polling à ~30 Hz (intervalle négatif = délai depuis la fin du callback) */
    add_repeating_timer_ms(-33, msp_timer_cb, NULL, &s_timer);

    s_initialized = true;
    LOGI("MSP interface initialisée (ch=%u, range=%u–%u µs, lipo_min=%u mV).",
         conf->enable_channel, conf->channel_range_min, conf->channel_range_max,
         conf->lipo_min_mv);
}

bool msp_poll_if_due(void) {
    if (!s_initialized || !s_msp_due)
        return false;
    s_msp_due = false;
    msp_poll_cycle();
    return true;
}

uint8_t msp_get_telemetry_record(uint8_t items, uint8_t *buf) {
    if (!s_tlm_ready)
        return 0u;
    s_tlm_ready = false;

    uint8_t *p = buf;

    /* Timestamp (toujours présent). */
    uint32_t ts = to_ms_since_boot(get_absolute_time());
    memcpy(p, &ts, sizeof(ts)); p += sizeof(ts);

    /* CH1-8 (16 B). */
    if (items & TLM_RC) {
        memcpy(p, s_rc_channels, sizeof(s_rc_channels));
        p += sizeof(s_rc_channels);
    }

    /* roll/pitch/yaw (6 B). */
    if (items & TLM_ATTITUDE) {
        memcpy(p, s_attitude, sizeof(s_attitude));
        p += sizeof(s_attitude);
    }

    /* fix/sats/lat/lon/alt/speed (14 B). */
    if (items & TLM_GPS) {
        *p++ = s_gps_fix;
        *p++ = s_gps_sats;
        memcpy(p, &s_gps_lat,   sizeof(s_gps_lat));   p += sizeof(s_gps_lat);
        memcpy(p, &s_gps_lon,   sizeof(s_gps_lon));   p += sizeof(s_gps_lon);
        memcpy(p, &s_gps_alt,   sizeof(s_gps_alt));   p += sizeof(s_gps_alt);
        memcpy(p, &s_gps_speed, sizeof(s_gps_speed)); p += sizeof(s_gps_speed);
    }

    /* vbat_mv/mAh/rssi/courant (7 B). */
    if (items & TLM_ANALOG) {
        memcpy(p, &s_vbat_mv,         sizeof(s_vbat_mv));         p += sizeof(s_vbat_mv);
        memcpy(p, &s_analog_mah,      sizeof(s_analog_mah));      p += sizeof(s_analog_mah);
        memcpy(p, &s_analog_rssi,     sizeof(s_analog_rssi));     p += sizeof(s_analog_rssi);
        memcpy(p, &s_analog_current,  sizeof(s_analog_current));  p += sizeof(s_analog_current);
    }

    return (uint8_t)(p - buf);
}

bool msp_is_lipo_connected(void) {
    return s_vbat_mv >= s_conf.lipo_min_mv;
}
