#ifndef LOG_H
#define LOG_H

#include <osapi.h>
#include <user_interface.h>

#include "config.h"
#include "missing-decls.h"

#define CRITICAL(s, ...) { \
    if (LOG_LEVEL <= LEVEL_CRITICAL) \
        os_printf("critical: %s:%d: " s, __FILE__, __LINE__, ##__VA_ARGS__); \
    while (1) os_delay_us(1000); \
}

#define ERROR(s, ...) { \
    if (LOG_LEVEL <= LEVEL_ERROR) \
        os_printf("error: %s:%d: " s, __FILE__, __LINE__, ##__VA_ARGS__); \
}

#define WARNING(s, ...) { \
    if (LOG_LEVEL <= LEVEL_WARNING) \
        os_printf("warning: %s:%d: " s, __FILE__, __LINE__, ##__VA_ARGS__); \
}

#define INFO(s, ...) { \
    if (LOG_LEVEL <= LEVEL_INFO) \
        os_printf("info: %s:%d: " s, __FILE__, __LINE__, ##__VA_ARGS__); \
}

#define DEBUG(s, ...) { \
    if (LOG_LEVEL <= LEVEL_DEBUG) \
        os_printf("debug: %s:%d: " s, __FILE__, __LINE__, ##__VA_ARGS__); \
}

#endif
