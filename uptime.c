#include <c_types.h>
#include <user_interface.h>

#include "log.h"
#include "uptime.h"

os_timer_t uptime_overflow_timer;

uint32_t uptime_high = 0;
uint32_t uptime_last = 0;

ICACHE_FLASH_ATTR void uptime_overflow_handler(void *arg) {
    (void)arg;

    uint32_t t;

    LOG_INFO(MAIN, "uptime_overflow_handler\n")

    if ((t=system_get_time()) < uptime_last)
        uptime_high++;

    uptime_last = t;
}

ICACHE_FLASH_ATTR uint64_t uptime_us() {
    return ((uint64_t)uptime_high<<32) | system_get_time();
}
