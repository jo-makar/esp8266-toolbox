#include "log/log.h"
#include "smtp/smtp.h"
#include "uptime.h"

#include <osapi.h>

os_timer_t uptime_timer;

static uint32_t uptime_high = 0;
static uint32_t uptime_last = 0;

ICACHE_FLASH_ATTR void uptime_handler(void *arg) {
    (void)arg;

    uint32_t t;
    uint32_t days1, days2;
    char subj[64];

    DEBUG(MAIN, "uptime_handler")

    days1 = (uptime_us() / 1000000) / (60*60*24);

    if ((t=system_get_time()) < uptime_last)
        uptime_high++;

    uptime_last = t;

    days2 = (uptime_us() / 1000000) / (60*60*24);
    if (days2 > days1) {
        os_snprintf(subj, sizeof(subj), "esp8266-%08x day %u", system_get_chip_id(), days2);
        smtp_send(smtp_server.from, smtp_server.to, subj, "");
    }
}

ICACHE_FLASH_ATTR uint64_t uptime_us() {
    return ((uint64_t)uptime_high<<32) | system_get_time();
}
