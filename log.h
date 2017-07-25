#ifndef LOG_H
#define LOG_H

#include <osapi.h>
#include <user_interface.h>

#include "config.h"
#include "missing-decls.h"

#define _PREFIX(level) { \
    uint32_t t = system_get_time(); \
    os_printf("%u.%03u: " level ": %s:%d: ", \
              t/1000000, (t%1000000)/1000, __FILE__, __LINE__); \
}

#define CRITICAL_PREFIX _PREFIX("critical")
#define ERROR_PREFIX _PREFIX("error")
#define WARNING_PREFIX _PREFIX("warning")
#define INFO_PREFIX _PREFIX("info")
#define DEBUG_PREFIX _PREFIX("debug")

#define CRITICAL(sys, fmt, ...) { \
    if (sys##_LOG_LEVEL <= LEVEL_CRITICAL) { \
        CRITICAL_PREFIX \
        os_printf(fmt, ##__VA_ARGS__); \
    } \
    \
    while (1) os_delay_us(1000); \
}

#define ERROR(sys, fmt, ...) { \
    if (sys##_LOG_LEVEL <= LEVEL_ERROR) { \
        ERROR_PREFIX \
        os_printf(fmt, ##__VA_ARGS__); \
    } \
}

#define WARNING(sys, fmt, ...) { \
    if (sys##_LOG_LEVEL <= LEVEL_WARNING) { \
        WARNING_PREFIX \
        os_printf(fmt, ##__VA_ARGS__); \
    } \
}

#define INFO(sys, fmt, ...) { \
    if (sys##_LOG_LEVEL <= LEVEL_INFO) { \
        INFO_PREFIX \
        os_printf(fmt, ##__VA_ARGS__); \
    } \
}

#define DEBUG(sys, fmt, ...) { \
    if (sys##_LOG_LEVEL <= LEVEL_DEBUG) { \
        DEBUG_PREFIX \
        os_printf(fmt, ##__VA_ARGS__); \
    } \
}

#endif
