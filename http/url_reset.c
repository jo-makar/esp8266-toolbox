#include "../log/log.h"
#include "private.h"

#include <ip_addr.h>
#include <espconn.h>
#include <osapi.h>

static os_timer_t reset_timer;
static void reset(void *arg);

ICACHE_FLASH_ATTR int http_url_reset(HttpClient *client) {
    HTTP_IGNORE_POSTDATA

    HTTP_OUTBUF_APPEND("HTTP/1.1 202 Accepted\r\n")
    HTTP_OUTBUF_APPEND("Connection: close\r\n")
    HTTP_OUTBUF_APPEND("Content-type: text/html\r\n")
    HTTP_OUTBUF_APPEND("Content-length: 48\r\n")
    HTTP_OUTBUF_APPEND("\r\n")
    HTTP_OUTBUF_APPEND("<html><body><h1>202 Accepted</h1></body></html>\n")

    if (espconn_send(client->conn, http_outbuf, http_outbuflen))
        ERROR(HTTP, "espconn_send() failed")

    os_timer_disarm(&reset_timer);
    os_timer_setfn(&reset_timer, reset, NULL);
    os_timer_arm(&reset_timer, 5000, false);

    return 1;
}

ICACHE_FLASH_ATTR static void reset(void *arg) {
    (void)arg;

    INFO(HTTP, "rebooting\n")
    system_restart();
}
