#include "log.h"
#include "uptime.h"

Log _log;

ICACHE_FLASH_ATTR void log_init() {
    os_bzero(_log.buf, sizeof(_log.buf));
    _log.lock = 0;

    #if LOG_URLBUF_ENABLE
        os_bzero(_log.urlbuf, sizeof(_log.urlbuf));
    #endif
}

ICACHE_FLASH_ATTR void _log_entry(const char *level, const char *file, int line,
                                  const char *entry) {
    uint64_t time_us, time_secs;
    uint32_t days;
    uint8_t hrs, mins, secs;
    uint16_t ms;

    char first;
    int len;

    time_us = uptime_us();
    time_secs = time_us / 1000000;

    days = time_secs / (60*60*24);
    hrs = (time_secs % (60*60*24)) / (60*60);
    mins = ((time_secs % (60*60*24)) % (60*60)) / 60;
    secs = ((time_secs % (60*60*24)) % (60*60)) % 60;
    ms = (time_us % 1000000) / 1000;

    os_printf("%ud %02u:%02u:%02u.%03u: %s: %s:%d: %s",
              days, hrs, mins, secs, ms, level, file, line, entry);

    #if LOG_URLBUF_ENABLE
        /* There are some quirks to the snprintf() implementation */
        first = _log.urlbuf[0];
        len = os_snprintf(_log.urlbuf, 1,
                          "%ud %02u:%02u:%02u.%03u: %s: %s:%d: %s",
                          days, hrs, mins, secs, ms, level, file, line, entry);
        _log.urlbuf[0] = first;

        os_memmove(_log.urlbuf + len, _log.urlbuf, sizeof(_log.urlbuf)-len);

        os_snprintf(_log.urlbuf, len+1,
                    "%ud %02u:%02u:%02u.%03u: %s: %s:%d %s",
                    days, hrs, mins, secs, ms, level, file, line, entry);
    #endif
}
