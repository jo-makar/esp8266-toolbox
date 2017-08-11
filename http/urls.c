#include <ip_addr.h>
#include <espconn.h>

#include "../log/log.h"
#include "private.h"

uint8_t http_outbuf[HTTP_OUTBUF_MAXLEN];
uint16_t http_outbuflen;

ICACHE_FLASH_ATTR int http_url_404(HttpClient *client) {
    HTTP_IGNORE_POSTDATA

    HTTP_OUTBUF_APPEND("HTTP/1.1 404 Not Found\r\n")
    HTTP_OUTBUF_APPEND("Connection: close\r\n")
    HTTP_OUTBUF_APPEND("Content-type: text/html\r\n")
    HTTP_OUTBUF_APPEND("Content-length: 49\r\n")
    HTTP_OUTBUF_APPEND("\r\n")
    HTTP_OUTBUF_APPEND("<html><body><h1>404 Not Found</h1></body></html>\n")

    if (espconn_send(client->conn, http_outbuf, http_outbuflen))
        ERROR(HTTP, "url_404: espconn_send() failed\n")

    return 1;
}

ICACHE_FLASH_ATTR int http_url_blank(HttpClient *client) {
    HTTP_IGNORE_POSTDATA

    HTTP_OUTBUF_APPEND("HTTP/1.1 200 OK\r\n")
    HTTP_OUTBUF_APPEND("Connection: close\r\n")
    HTTP_OUTBUF_APPEND("Content-type: text/html\r\n")
    HTTP_OUTBUF_APPEND("Content-length: 27\r\n")
    HTTP_OUTBUF_APPEND("\r\n")
    HTTP_OUTBUF_APPEND("<html><body></body></html>\n")

    if (espconn_send(client->conn, http_outbuf, http_outbuflen))
        ERROR(HTTP, "url_blank: espconn_send() failed\n")

    return 1;
}

const HttpUrl http_urls[] = {
    { "/",           http_url_blank },
    { "/logs",       http_url_logs },
    { "/logs/lower", http_url_logs_lower },
    { "/logs/raise", http_url_logs_raise },
    { "/ota/bin",    http_url_ota_bin },
    { "/ota/push",   http_url_ota_push },
    { "/reset",      http_url_reset },
    { "/smtp/setup", http_url_smtp_setup },
    { "/uptime",     http_url_uptime },
    { "/version",    http_url_version },
    { "/wifi/setup", http_url_wifi_setup },
};

const size_t http_urlcount = sizeof(http_urls) / sizeof(*http_urls);
