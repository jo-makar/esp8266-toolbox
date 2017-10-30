#include "log.h"
#include "uptime.h"

typedef struct {
    char *sys;
    int level;
} LogLevel;

const LogLevel log_levels[] = { {"main", LOG_LEVEL_INFO},
                                {"smtp", LOG_LEVEL_INFO},
                              };

char log_line[LOG_LINE_LEN];

ICACHE_FLASH_ATTR int log_level(const char *sys) {
    size_t i;

    for (i=0; i<sizeof(log_levels)/sizeof(*log_levels); i++) {
        if (strcmp(sys, log_levels[i].sys) == 0)
            return log_levels[i].level;
    }

    return 0;
}

ICACHE_FLASH_ATTR void log_entry(const char *level, const char *file, int line,
                                 const char *msg) {
    uint64_t total_us, total_secs;

    uint32_t days;
    uint8_t hrs, mins, secs;
    uint16_t ms;

    total_us   = uptime_us();
    total_secs = total_us / 1000000;

    days = total_secs / (60*60*24);
    hrs  = (total_secs % (60*60*24)) / (60*60);
    mins = ((total_secs % (60*60*24)) % (60*60)) / 60;
    secs = ((total_secs % (60*60*24)) % (60*60)) % 60;
    ms   = (total_us % 1000000) / 1000;

    if (days > 0)
        os_printf("%ud %02u:%02u:%02u.%03u: %s: %s:%d: %s\n",
                  days, hrs, mins, secs, ms, level, file, line, msg);
    else
        os_printf("%02u:%02u:%02u.%03u: %s: %s:%d: %s\n",
                  hrs, mins, secs, ms, level, file, line, msg);
}
