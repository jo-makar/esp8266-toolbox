#include "../missing-decls.h"
#include "log.h"

#include <ets_sys.h>
#include <osapi.h>
#include <user_interface.h>

LogBuf logbuf;

ICACHE_FLASH_ATTR void log_entry(const char *level, const char *file, int line, const char *mesg) {
    uint64_t time_us, time_secs;
    uint32_t days;
    uint8_t hrs, mins, secs;
    uint16_t ms;
    int rv;
    size_t len;

    time_us   = system_get_time();
    time_secs = time_us / 1000000;

    days = time_secs / (60*60*24);
    hrs  = (time_secs % (60*60*24)) / (60*60);
    mins = ((time_secs % (60*60*24)) % (60*60)) / 60;
    secs = ((time_secs % (60*60*24)) % (60*60)) % 60;
    ms   = (time_us % 1000000) / 1000;

    if (days > 0)
        rv = os_snprintf(logbuf.entry, sizeof(logbuf.entry),
                         "%ud %02u:%02u:%02u.%03u: %s: %s:%d: %s\n",
                         days, hrs, mins, secs, ms, level, file, line, mesg);
    else
        rv = os_snprintf(logbuf.entry, sizeof(logbuf.entry),
                        "%02u:%02u:%02u.%03u: %s: %s:%d: %s\n",
                        hrs, mins, secs, ms, level, file, line, mesg);

    if ((size_t)rv >= sizeof(logbuf.entry))
        os_strncpy(logbuf.entry + (sizeof(logbuf.entry)-5), "...\n", 5);

    os_printf("%s", logbuf.entry);

    len = os_strlen(logbuf.entry) + 1;
    os_memmove(logbuf.main + len, logbuf.main, sizeof(logbuf.main)-len);
    os_memcpy(logbuf.main, logbuf.entry, len);
}
