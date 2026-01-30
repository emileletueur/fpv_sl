#pragma once

#include <stdarg.h>

void debug_log_vprintf(const char *fmt, va_list args);
void debug_log_printf(const char *fmt, ...);

/* =======================
 * Log levels
 * ======================= */
#define LOG_LEVEL_NONE  0
#define LOG_LEVEL_ERR   1
#define LOG_LEVEL_WARN  2
#define LOG_LEVEL_INFO  3
#define LOG_LEVEL_DBG   4

#ifndef LOG_LEVEL
#define LOG_LEVEL LOG_LEVEL_INFO
#endif

#ifdef DEBUG_LOG_ENABLE

#if LOG_LEVEL >= LOG_LEVEL_ERR
#define LOGE(fmt, ...) \
    debug_log_printf("[ERR] " fmt "\r\n", ##__VA_ARGS__)
#else
#define LOGE(...) do {} while (0)
#endif

#if LOG_LEVEL >= LOG_LEVEL_WARN
#define LOGW(fmt, ...) \
    debug_log_printf("[WRN] " fmt "\r\n", ##__VA_ARGS__)
#else
#define LOGW(...) do {} while (0)
#endif

#if LOG_LEVEL >= LOG_LEVEL_INFO
#define LOGI(fmt, ...) \
    debug_log_printf("[INF] " fmt "\r\n", ##__VA_ARGS__)
#else
#define LOGI(...) do {} while (0)
#endif

#if LOG_LEVEL >= LOG_LEVEL_DBG
#define LOGD(fmt, ...) \
    debug_log_printf("[DBG] " fmt "\r\n", ##__VA_ARGS__)
#else
#define LOGD(...) do {} while (0)
#endif

#else

#define LOGE(...) do {} while (0)
#define LOGW(...) do {} while (0)
#define LOGI(...) do {} while (0)
#define LOGD(...) do {} while (0)

#endif
