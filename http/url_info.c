#include "../log/log.h"
#include "../uptime.h"
#include "private.h"

#include <ip_addr.h>
#include <espconn.h>
#include <osapi.h>

ICACHE_FLASH_ATTR int http_url_uptime(HttpClient *client) {
    size_t beg, end;
    uint64_t uptime_secs;
    uint32_t days; 
    uint8_t hours, mins, secs;

    HTTP_IGNORE_POSTDATA

    HTTP_OUTBUF_APPEND("HTTP/1.1 200 OK\r\n")
    HTTP_OUTBUF_APPEND("Connection: close\r\n")
    HTTP_OUTBUF_APPEND("Content-type: text/html\r\n")

    #if HTTP_OUTBUF_MAXLEN > 9999
        #error "Content-length template requires adjustment"
    #endif

    HTTP_OUTBUF_APPEND("Content-length: xxxx\r\n")
    HTTP_OUTBUF_APPEND("\r\n")

    beg = http_outbuflen;

    HTTP_OUTBUF_APPEND("<html><body>")

    uptime_secs = uptime_us() / 1000000;
    days = uptime_secs / (60*60*24);
    hours = (uptime_secs % (60*60*24)) / (60*60);
    mins = ((uptime_secs % (60*60*24)) % (60*60)) / 60;
    secs = ((uptime_secs % (60*60*24)) % (60*60)) % 60;

    HTTP_OUTBUF_APPEND("<h1>Uptime of ")
    if (days > 0)
        HTTP_OUTBUF_PRINTF("%u days ", days)
    HTTP_OUTBUF_PRINTF("%02u:%02u:%02u</h1>", hours, mins, secs)

    HTTP_OUTBUF_APPEND("</body></html>\n")

    end = http_outbuflen;

    os_snprintf((char *)http_outbuf+77, 5, "%4u", end-beg);
    http_outbuf[81] = '\r';

    if (espconn_send(client->conn, http_outbuf, http_outbuflen))
        ERROR(HTTP, "espconn_send() failed")

    return 1;
}

ICACHE_FLASH_ATTR int http_url_version(HttpClient *client) {
    size_t beg, end;

    HTTP_IGNORE_POSTDATA

    HTTP_OUTBUF_APPEND("HTTP/1.1 200 OK\r\n")
    HTTP_OUTBUF_APPEND("Connection: close\r\n")
    HTTP_OUTBUF_APPEND("Content-type: text/html\r\n")

    #if HTTP_OUTBUF_MAXLEN > 9999
        #error "Content-length template requires adjustment"
    #endif

    HTTP_OUTBUF_APPEND("Content-length: xxxx\r\n")
    HTTP_OUTBUF_APPEND("\r\n")

    beg = http_outbuflen;

    HTTP_OUTBUF_APPEND("<html><body>")
    HTTP_OUTBUF_PRINTF("<h1>Version %s built on %s</h1>", VERSION, BUILD_DATETIME)
    HTTP_OUTBUF_APPEND("</body></html>\n")

    end = http_outbuflen;

    os_snprintf((char *)http_outbuf+77, 5, "%4u", end-beg);
    http_outbuf[81] = '\r';

    if (espconn_send(client->conn, http_outbuf, http_outbuflen))
        ERROR(HTTP, "espconn_send() failed")

    return 1;
}
