#include "msp_interface.h"

#include "debug_log.h"
#include "msp_driver.h"
#include "pico/time.h"

#include <string.h>

/* Function IDs MSP v2 — identiques aux cmd v1 pour les messages standard Betaflight */
#define MSP_CMD_STATUS ((uint16_t)101u)
#define MSP_CMD_RC     ((uint16_t)105u)
#define MSP_CMD_ANALOG ((uint16_t)110u)

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

static bool     s_armed        = false;
static bool     s_enable_active = false;
static uint16_t s_vbat_mv      = 0;

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

    /* ── MSP_RC — canal ENABLE ── */
    msp_send_request(&s_drv, MSP_CMD_RC);
    if (msp_read_response(&s_drv, MSP_CMD_RC, buf, &len) == 0) {
        uint8_t ch_idx = s_conf.enable_channel - 1u;
        uint16_t val   = 0;
        if (len >= (uint8_t)((ch_idx + 1u) * 2u))
            memcpy(&val, buf + ch_idx * 2u, sizeof(val));
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

    /* ── MSP_ANALOG — tension LiPo ── */
    msp_send_request(&s_drv, MSP_CMD_ANALOG);
    if (msp_read_response(&s_drv, MSP_CMD_ANALOG, buf, &len) == 0 && len >= 1u) {
        s_vbat_mv = (uint16_t)buf[0] * 100u;
        LOGD("MSP: vbat=%u mV.", s_vbat_mv);
    }
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

void msp_poll_if_due(void) {
    if (!s_initialized || !s_msp_due)
        return;
    s_msp_due = false;
    msp_poll_cycle();
}

bool msp_is_lipo_connected(void) {
    return s_vbat_mv >= s_conf.lipo_min_mv;
}
