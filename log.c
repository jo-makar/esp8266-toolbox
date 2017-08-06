#include "log.h"

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
    uint32_t time;
    char first;
    int len;

    time = system_get_time();

    os_printf("%u.%03u: %s: %s:%d: %s",
                time/1000000, (time%1000000)/1000,
                level, file, line, entry);

    #if LOG_URLBUF_ENABLE
        /* There are some quirks to the snprintf() implementation */
        first = _log.urlbuf[0];
        len = os_snprintf(_log.urlbuf, 1, "%u.%03u: %s: %s:%d: %s",
                                            time/1000000, (time%1000000)/1000,
                                            level, file, line, entry);
        _log.urlbuf[0] = first;

        os_memmove(_log.urlbuf + len, _log.urlbuf, sizeof(_log.urlbuf)-len);

        os_snprintf(_log.urlbuf, len+1, "%u.%03u: %s: %s:%d %s",
                                        time/1000000, (time%1000000)/1000,
                                        level, file, line, entry);
    #endif
}
