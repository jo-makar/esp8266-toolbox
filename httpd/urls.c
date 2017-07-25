#include <ip_addr.h> /* Must be included before espconn.h */

#include <espconn.h>

#include "httpd.h"

uint8_t httpd_outbuf[HTTPD_OUTBUF_MAXLEN];
uint16_t httpd_outbuflen;

ICACHE_FLASH_ATTR int httpd_url_404(HttpdClient *client) {
    HTTPD_IGNORE_POSTDATA

    HTTPD_OUTBUF_APPEND("HTTP/1.1 404 Not Found\r\n")
    HTTPD_OUTBUF_APPEND("Connection: close\r\n")
    HTTPD_OUTBUF_APPEND("Content-type: text/html\r\n")
    HTTPD_OUTBUF_APPEND("Content-length: 48\r\n")
    HTTPD_OUTBUF_APPEND("\r\n")
    HTTPD_OUTBUF_APPEND("<html><body><h1>404 Not Found</h1></body></html>")

    if (espconn_send(client->conn, httpd_outbuf, httpd_outbuflen))
        ERROR(HTTPD, "url_404: espconn_send() failed\n")

    return 1;
}

ICACHE_FLASH_ATTR int httpd_url_blank(HttpdClient *client) {
    HTTPD_IGNORE_POSTDATA

    HTTPD_OUTBUF_APPEND("HTTP/1.1 200 OK\r\n")
    HTTPD_OUTBUF_APPEND("Connection: close\r\n")
    HTTPD_OUTBUF_APPEND("Content-type: text/html\r\n")
    HTTPD_OUTBUF_APPEND("Content-length: 26\r\n")
    HTTPD_OUTBUF_APPEND("\r\n")
    HTTPD_OUTBUF_APPEND("<html><body></body></html>")

    if (espconn_send(client->conn, httpd_outbuf, httpd_outbuflen))
        ERROR(HTTPD, "url_blank: espconn_send() failed\n")

    return 1;
}

const HttpdUrl httpd_urls[] = {
    { "/",          httpd_url_blank },
    { "/fota/push", httpd_url_fota },
};

const size_t httpd_urlcount = sizeof(httpd_urls) / sizeof(*httpd_urls);
