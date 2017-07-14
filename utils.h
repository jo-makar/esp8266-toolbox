#ifndef UTILS_H
#define UTILS_H

#include <osapi.h>
#include <user_interface.h>

#include "missing-decls.h"

#define FAIL(s, ...) { \
    os_printf("%s:%d: " s, __FILE__, __LINE__, ##__VA_ARGS__); \
    while (1) os_delay_us(1000); \
}

#endif
