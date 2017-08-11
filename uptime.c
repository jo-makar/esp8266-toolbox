#include "log/log.h"

#include <osapi.h>

os_timer_t uptime_timer;

static uint32_t uptime_high = 0;
static uint32_t uptime_last = 0;

ICACHE_FLASH_ATTR void uptime_handler(void *arg) {
    (void)arg;

    uint32_t t;

    DEBUG(MAIN, "uptime_handler")

    if ((t=system_get_time()) < uptime_last)
        uptime_high++;

    uptime_last = t;
}

ICACHE_FLASH_ATTR uint64_t uptime_us() {
    return ((uint64_t)uptime_high<<32) | system_get_time();
}
