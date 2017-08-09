#ifndef LOG_H
#define LOG_H

#include <osapi.h>
#include <user_interface.h>

#include "httpd/httpd.h"
#include "config.h"
#include "missing-decls.h"

typedef struct {
    char buf[256];
    uint8_t lock;

    #if LOG_URLBUF_ENABLE
        char urlbuf[HTTPD_OUTBUF_MAXLEN*3/4];
    #endif
} Log;
extern Log _log;

void log_init();

void _log_entry(const char *level, const char *file, int line,
                const char *entry);

#define _LOG_LOCK { \
    int i; \
    \
    _log.lock++; \
    for (i=0; _log.lock>1; i++) { \
        if (i>0 && i%10==0) \
            system_soft_wdt_feed(); \
        os_delay_us(1000); \
    } \
}

#define _LOG_UNLOCK _log.lock--;

#define LOG_CRITICAL(sys, fmt, ...) { \
    int i; \
    \
    if (sys##_LOG_LEVEL <= LEVEL_CRITICAL) { \
        _LOG_LOCK \
        \
        os_snprintf(_log.buf, sizeof(_log.buf), fmt, ##__VA_ARGS__); \
        _log.buf[sizeof(_log.buf)-1] = 0; \
        _log_entry("critical", __FILE__, __LINE__, _log.buf); \
        \
        _LOG_UNLOCK \
    } \
    \
    for (i=0; ; i++) { \
        if (i>0 && i%10==0) \
            system_soft_wdt_feed(); \
        os_delay_us(1000); \
    } \
}

#define LOG_ERROR(sys, fmt, ...) { \
    if (sys##_LOG_LEVEL <= LEVEL_ERROR) { \
        _LOG_LOCK \
        \
        os_snprintf(_log.buf, sizeof(_log.buf), fmt, ##__VA_ARGS__); \
        _log.buf[sizeof(_log.buf)-1] = 0; \
        _log_entry("error", __FILE__, __LINE__, _log.buf); \
        \
        _LOG_UNLOCK \
    } \
}

#define LOG_WARNING(sys, fmt, ...) { \
    if (sys##_LOG_LEVEL <= LEVEL_WARNING) { \
        _LOG_LOCK \
        \
        os_snprintf(_log.buf, sizeof(_log.buf), fmt, ##__VA_ARGS__); \
        _log.buf[sizeof(_log.buf)-1] = 0; \
        _log_entry("warning", __FILE__, __LINE__, _log.buf); \
        \
        _LOG_UNLOCK \
    } \
}

#define LOG_INFO(sys, fmt, ...) { \
    if (sys##_LOG_LEVEL <= LEVEL_INFO) { \
        _LOG_LOCK \
        \
        os_snprintf(_log.buf, sizeof(_log.buf), fmt, ##__VA_ARGS__); \
        _log.buf[sizeof(_log.buf)-1] = 0; \
        _log_entry("info", __FILE__, __LINE__, _log.buf); \
        \
        _LOG_UNLOCK \
    } \
}

#define LOG_DEBUG(sys, fmt, ...) { \
    if (sys##_LOG_LEVEL <= LEVEL_DEBUG) { \
        _LOG_LOCK \
        \
        os_snprintf(_log.buf, sizeof(_log.buf), fmt, ##__VA_ARGS__); \
        _log.buf[sizeof(_log.buf)-1] = 0; \
        _log_entry("debug", __FILE__, __LINE__, _log.buf); \
        \
        _LOG_UNLOCK \
    } \
}

#endif
