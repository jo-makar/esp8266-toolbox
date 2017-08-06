#ifndef LOG_H
#define LOG_H

#include <osapi.h>
#include <user_interface.h>

#include "config.h"
#include "missing-decls.h"

typedef struct {
    char buf[256];
    uint8_t lock;

    #if LOG_URLBUF_ENABLE
        char urlbuf[3*1024];
    #endif
} Log;
extern Log _log;

void log_init();

void _log_entry(const char *level, const char *file, int line,
                uint8_t sys, const char *entry);

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
    _LOG_LOCK \
    \
    os_snprintf(_log.buf, sizeof(_log.buf), fmt, ##__VA_ARGS__); \
    _log.buf[sizeof(_log.buf)-1] = 0; \
    _log_entry("critical", __FILE__, __LINE__, sys##_LOG_LEVEL, _log.buf); \
    \
    _LOG_UNLOCK \
    \
    for (i=0; ; i++) { \
        if (i>0 && i%10==0) \
            system_soft_wdt_feed(); \
        os_delay_us(1000); \
    } \
}

#define LOG_ERROR(sys, fmt, ...) { \
    _LOG_LOCK \
    \
    os_snprintf(_log.buf, sizeof(_log.buf), fmt, ##__VA_ARGS__); \
    _log.buf[sizeof(_log.buf)-1] = 0; \
    _log_entry("error", __FILE__, __LINE__, sys##_LOG_LEVEL, _log.buf); \
    \
    _LOG_UNLOCK \
}

#define LOG_WARNING(sys, fmt, ...) { \
    _LOG_LOCK \
    \
    os_snprintf(_log.buf, sizeof(_log.buf), fmt, ##__VA_ARGS__); \
    _log.buf[sizeof(_log.buf)-1] = 0; \
    _log_entry("warning", __FILE__, __LINE__, sys##_LOG_LEVEL, _log.buf); \
    \
    _LOG_UNLOCK \
}

#define LOG_INFO(sys, fmt, ...) { \
    _LOG_LOCK \
    \
    os_snprintf(_log.buf, sizeof(_log.buf), fmt, ##__VA_ARGS__); \
    _log.buf[sizeof(_log.buf)-1] = 0; \
    _log_entry("info", __FILE__, __LINE__, sys##_LOG_LEVEL, _log.buf); \
    \
    _LOG_UNLOCK \
}

#define LOG_DEBUG(sys, fmt, ...) { \
    _LOG_LOCK \
    \
    os_snprintf(_log.buf, sizeof(_log.buf), fmt, ##__VA_ARGS__); \
    _log.buf[sizeof(_log.buf)-1] = 0; \
    _log_entry("debug", __FILE__, __LINE__, sys##_LOG_LEVEL, _log.buf); \
    \
    _LOG_UNLOCK \
}

#endif
