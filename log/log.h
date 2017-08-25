#ifndef LOG_H
#define LOG_H

#include "../config.h"
#include "../missing-decls.h"
#include "private.h"

#include <osapi.h>

extern uint8_t CRYPTO_LOG_LEVEL;
extern uint8_t DRIVER_LOG_LEVEL;
extern uint8_t HTTP_LOG_LEVEL;
extern uint8_t MAIN_LOG_LEVEL;
extern uint8_t OTA_LOG_LEVEL;
extern uint8_t SMTP_LOG_LEVEL;

#define LEVEL_DEBUG    10
#define LEVEL_INFO     20
#define LEVEL_WARNING  30
#define LEVEL_ERROR    40
#define LEVEL_CRITICAL 50

void log_init();

void log_raise(uint8_t *sys);
void log_lower(uint8_t *sys);

#define CRITICAL(sys, fmt, ...) { \
    int rv, i; \
    \
    if (sys##_LOG_LEVEL <= LEVEL_CRITICAL) { \
        rv = os_snprintf(logbuf.line, sizeof(logbuf.line), fmt, ##__VA_ARGS__); \
        if ((size_t)rv >= sizeof(logbuf.line)) \
            os_strncpy(logbuf.line + (sizeof(logbuf.line)-4), "...", 4); \
        log_entry("critical", __FILE__, __LINE__, logbuf.line); \
    } \
    \
    for (i=0; ; i++) { \
        if (i>0 && i%10==0) \
            system_soft_wdt_feed(); \
        os_delay_us(1000); \
    } \
}

#define ERROR(sys, fmt, ...) { \
    int rv; \
    \
    if (sys##_LOG_LEVEL <= LEVEL_ERROR) { \
        rv = os_snprintf(logbuf.line, sizeof(logbuf.line), fmt, ##__VA_ARGS__); \
        if ((size_t)rv >= sizeof(logbuf.line)) \
            os_strncpy(logbuf.line + (sizeof(logbuf.line)-4), "...", 4); \
        log_entry("error", __FILE__, __LINE__, logbuf.line); \
    } \
}

#define WARNING(sys, fmt, ...) { \
    int rv; \
    \
    if (sys##_LOG_LEVEL <= LEVEL_WARNING) { \
        rv = os_snprintf(logbuf.line, sizeof(logbuf.line), fmt, ##__VA_ARGS__); \
        if ((size_t)rv >= sizeof(logbuf.line)) \
            os_strncpy(logbuf.line + (sizeof(logbuf.line)-4), "...", 4); \
        log_entry("warning", __FILE__, __LINE__, logbuf.line); \
    } \
}

#define INFO(sys, fmt, ...) { \
    int rv; \
    \
    if (sys##_LOG_LEVEL <= LEVEL_INFO) { \
        rv = os_snprintf(logbuf.line, sizeof(logbuf.line), fmt, ##__VA_ARGS__); \
        if ((size_t)rv >= sizeof(logbuf.line)) \
            os_strncpy(logbuf.line + (sizeof(logbuf.line)-4), "...", 4); \
        log_entry("info", __FILE__, __LINE__, logbuf.line); \
    } \
}

#define DEBUG(sys, fmt, ...) { \
    int rv; \
    \
    if (sys##_LOG_LEVEL <= LEVEL_DEBUG) { \
        rv = os_snprintf(logbuf.line, sizeof(logbuf.line), fmt, ##__VA_ARGS__); \
        if ((size_t)rv >= sizeof(logbuf.line)) \
            os_strncpy(logbuf.line + (sizeof(logbuf.line)-4), "...", 4); \
        log_entry("debug", __FILE__, __LINE__, logbuf.line); \
    } \
}

#endif
