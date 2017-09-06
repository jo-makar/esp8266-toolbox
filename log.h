#ifndef LOG_H
#define LOG_H

#include "missing-decls.h"

#include <osapi.h>
#include <stdlib.h>

#define LOG_LEVEL_DEBUG    10
#define LOG_LEVEL_INFO     20
#define LOG_LEVEL_WARNING  30
#define LOG_LEVEL_ERROR    40
#define LOG_LEVEL_CRITICAL 50

void log_entry(const char *level, const char *file, int line, const char *msg);
int log_level(const char *sys);

#define LOG_LINE_LEN 128
extern char log_line[LOG_LINE_LEN];

#define log_debug(sys, fmt, ...) do { \
    int rv; \
    \
    if (log_level(sys) <= LOG_LEVEL_DEBUG) { \
        rv = os_snprintf(log_line, sizeof(log_line), fmt, ##__VA_ARGS__); \
        if ((size_t)rv >= sizeof(log_line)) \
            os_strncpy(log_line + (sizeof(log_line)-4), "...", 4); \
        log_entry("debug", __FILE__, __LINE__, log_line); \
    } \
} while (0)

#define log_info(sys, fmt, ...) do { \
    int rv; \
    \
    if (log_level(sys) <= LOG_LEVEL_INFO) { \
        rv = os_snprintf(log_line, sizeof(log_line), fmt, ##__VA_ARGS__); \
        if ((size_t)rv >= sizeof(log_line)) \
            os_strncpy(log_line + (sizeof(log_line)-4), "...", 4); \
        log_entry("info", __FILE__, __LINE__, log_line); \
    } \
} while (0)

#define log_warning(sys, fmt, ...) do { \
    int rv; \
    \
    if (log_level(sys) <= LOG_LEVEL_WARNING) { \
        rv = os_snprintf(log_line, sizeof(log_line), fmt, ##__VA_ARGS__); \
        if ((size_t)rv >= sizeof(log_line)) \
            os_strncpy(log_line + (sizeof(log_line)-4), "...", 4); \
        log_entry("warning", __FILE__, __LINE__, log_line); \
    } \
} while (0)

#define log_error(sys, fmt, ...) do { \
    int rv; \
    \
    if (log_level(sys) <= LOG_LEVEL_ERROR) { \
        rv = os_snprintf(log_line, sizeof(log_line), fmt, ##__VA_ARGS__); \
        if ((size_t)rv >= sizeof(log_line)) \
            os_strncpy(log_line + (sizeof(log_line)-4), "...", 4); \
        log_entry("error", __FILE__, __LINE__, log_line); \
    } \
} while (0)

#define log_critical(sys, fmt, ...) do { \
    int i, rv; \
    \
    if (log_level(sys) <= LOG_LEVEL_CRITICAL) { \
        rv = os_snprintf(log_line, sizeof(log_line), fmt, ##__VA_ARGS__); \
        if ((size_t)rv >= sizeof(log_line)) \
            os_strncpy(log_line + (sizeof(log_line)-4), "...", 4); \
        log_entry("critical", __FILE__, __LINE__, log_line); \
    } \
    \
    for (i=0; ; i++) { \
        if (i > 0 && i % 10 == 0) \
            system_soft_wdt_feed(); \
        os_delay_us(1000); \
    } \
} while (0)

#endif
