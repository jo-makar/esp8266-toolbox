#include <ip_addr.h> /* Must be included before espconn.h */

#include <espconn.h>
#include <osapi.h>

#include "../log.h"
#include "httpd.h"

static os_timer_t reset_timer;
static void reset(void *arg);

ICACHE_FLASH_ATTR int httpd_url_reset(HttpdClient *client) {
    HTTPD_IGNORE_POSTDATA

    HTTPD_OUTBUF_APPEND("HTTP/1.1 202 Accepted\r\n")
    HTTPD_OUTBUF_APPEND("Connection: close\r\n")
    HTTPD_OUTBUF_APPEND("Content-type: text/html\r\n")
    HTTPD_OUTBUF_APPEND("Content-length: 48\r\n")
    HTTPD_OUTBUF_APPEND("\r\n")
    HTTPD_OUTBUF_APPEND("<html><body><h1>202 Accepted</h1></body></html>\n")

    if (espconn_send(client->conn, httpd_outbuf, httpd_outbuflen))
        LOG_ERROR(HTTPD, "espconn_send() failed\n")

    os_timer_disarm(&reset_timer);
    os_timer_setfn(&reset_timer, reset, NULL);
    os_timer_arm(&reset_timer, 5000, false);

    return 1;
}

ICACHE_FLASH_ATTR static void reset(void *arg) {
    (void)arg;

    LOG_INFO(FOTA, "rebooting\n")
    system_restart();
}
