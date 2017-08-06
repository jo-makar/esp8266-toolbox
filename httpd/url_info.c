#include <ip_addr.h> /* Must be included before espconn.h */

#include <espconn.h>

#include "../config.h"
#include "../uptime.h"
#include "httpd.h"

ICACHE_FLASH_ATTR int httpd_url_version(HttpdClient *client) {
    HTTPD_IGNORE_POSTDATA

    size_t beg, end;

    HTTPD_OUTBUF_APPEND("HTTP/1.1 200 OK\r\n")
    HTTPD_OUTBUF_APPEND("Connection: close\r\n")
    HTTPD_OUTBUF_APPEND("Content-type: text/html\r\n")

    #if HTTPD_OUTBUF_MAXLEN > 9999
        #error "Content-length template requires adjustment"
    #endif

    HTTPD_OUTBUF_APPEND("Content-length: xxxx\r\n")
    HTTPD_OUTBUF_APPEND("\r\n")

    beg = httpd_outbuflen;

    HTTPD_OUTBUF_APPEND("<html><body>")

    HTTPD_OUTBUF_PRINTF("<h1>Version %u.%u.%u built on %s</h1>",
                        VERSION_MAJOR, VERSION_MINOR, VERSION_PATCH,
                        BUILD_DATETIME)

    HTTPD_OUTBUF_APPEND("</body></html>\n")

    end = httpd_outbuflen;

    /*
     * Write the Content-length value
     *
     * Apparently os_snprintf() behaves differently than snprintf(),
     * it os_snprintf() always writes the terminating null byte
     */
    os_snprintf((char *)httpd_outbuf+77, 5, "%4u", end-beg);
    httpd_outbuf[81] = '\r';

    if (espconn_send(client->conn, httpd_outbuf, httpd_outbuflen))
        LOG_ERROR(HTTPD, "espconn_send() failed\n")

    return 1;
}

ICACHE_FLASH_ATTR int httpd_url_uptime(HttpdClient *client) {
    HTTPD_IGNORE_POSTDATA

    size_t beg, end;
    uint64_t uptime_secs;
    uint32_t days; 
    uint8_t hours, mins, secs;

    HTTPD_OUTBUF_APPEND("HTTP/1.1 200 OK\r\n")
    HTTPD_OUTBUF_APPEND("Connection: close\r\n")
    HTTPD_OUTBUF_APPEND("Content-type: text/html\r\n")

    #if HTTPD_OUTBUF_MAXLEN > 9999
        #error "Content-length template requires adjustment"
    #endif

    HTTPD_OUTBUF_APPEND("Content-length: xxxx\r\n")
    HTTPD_OUTBUF_APPEND("\r\n")

    beg = httpd_outbuflen;

    HTTPD_OUTBUF_APPEND("<html><body>")

    uptime_secs = uptime_us() / 1000000;
    days = uptime_secs / (60*60*24);
    hours = (uptime_secs % (60*60*24)) / (60*60);
    mins = ((uptime_secs % (60*60*24)) % (60*60)) / 60;
    secs = ((uptime_secs % (60*60*24)) % (60*60)) % 60;

    HTTPD_OUTBUF_APPEND("<h1>Uptime of ")
    if (days > 0)
        HTTPD_OUTBUF_PRINTF("%u days ", days)
    HTTPD_OUTBUF_PRINTF("%02u:%02u:%02u</h1>", hours, mins, secs)

    HTTPD_OUTBUF_APPEND("</body></html>\n")

    end = httpd_outbuflen;

    /*
     * Write the Content-length value
     *
     * Apparently os_snprintf() behaves differently than snprintf(),
     * it os_snprintf() always writes the terminating null byte
     */
    os_snprintf((char *)httpd_outbuf+77, 5, "%4u", end-beg);
    httpd_outbuf[81] = '\r';

    if (espconn_send(client->conn, httpd_outbuf, httpd_outbuflen))
        LOG_ERROR(HTTPD, "espconn_send() failed\n")

    return 1;
}
