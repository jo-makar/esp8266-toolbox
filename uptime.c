#include "missing-decls.h"
#include "uptime.h"

#include <osapi.h>

struct {
    int init;
    uint32_t high;
    uint32_t last;
    os_timer_t timer;
} uptime_state;

void uptime_handler(void *arg);

ICACHE_FLASH_ATTR void uptime_init() {
    uptime_state.high = 0;
    uptime_state.last = 0;

    os_timer_disarm(&uptime_state.timer);
    os_timer_setfn(&uptime_state.timer, uptime_handler, NULL);
    os_timer_arm(&uptime_state.timer, 1000*60*20, true);

    uptime_state.init = 1;
}

ICACHE_FLASH_ATTR uint64_t uptime_us() {
    if (uptime_state.init == 0)
        uptime_init();

    /*
     * Use uptime_state.last here rather than system_get_time()
     * so that the system_get_time() overflow is handled cleanly.
     */
    uptime_handler(NULL);
    return ((uint64_t)uptime_state.high<<32) | uptime_state.last;
}

ICACHE_FLASH_ATTR void uptime_handler(void *arg) {
    (void)arg;

    uint32_t t;

    if ((t = system_get_time()) < uptime_state.last)
        uptime_state.high++;

    uptime_state.last = t;
}

