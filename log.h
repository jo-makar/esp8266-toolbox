#ifndef LOG_H
#define LOG_H

#include <osapi.h>
#include <user_interface.h>

#include "config.h"
#include "missing-decls.h"

#define CRITICAL(s, ...) { \
    uint32_t t = system_get_time(); \
    \
    if (LOG_LEVEL <= LEVEL_CRITICAL) \
        os_printf("%u.%03u: critical: %s:%d: " s, \
                  t/1000000, (t%1000000)/1000, \
                  __FILE__, __LINE__, ##__VA_ARGS__); \
    \
    while (1) os_delay_us(1000); \
}

#define ERROR(s, ...) { \
    uint32_t t = system_get_time(); \
    \
    if (LOG_LEVEL <= LEVEL_ERROR) \
        os_printf("%u.%03u: error: %s:%d: " s, \
                  t/1000000, (t%1000000)/1000, \
                  __FILE__, __LINE__, ##__VA_ARGS__); \
}

#define WARNING(s, ...) { \
    uint32_t t = system_get_time(); \
    \
    if (LOG_LEVEL <= LEVEL_WARNING) \
        os_printf("%u.%03u: warning: %s:%d: " s, \
                  t/1000000, (t%1000000)/1000, \
                  __FILE__, __LINE__, ##__VA_ARGS__); \
}

#define INFO(s, ...) { \
    uint32_t t = system_get_time(); \
    \
    if (LOG_LEVEL <= LEVEL_INFO) \
        os_printf("%u.%03u: info: %s:%d: " s, \
                  t/1000000, (t%1000000)/1000, \
                  __FILE__, __LINE__, ##__VA_ARGS__); \
}

#define DEBUG(s, ...) { \
    uint32_t t = system_get_time(); \
    \
    if (LOG_LEVEL <= LEVEL_DEBUG) \
        os_printf("%u.%03u: debug: %s:%d: " s, \
                  t/1000000, (t%1000000)/1000, \
                  __FILE__, __LINE__, ##__VA_ARGS__); \
}

#endif
