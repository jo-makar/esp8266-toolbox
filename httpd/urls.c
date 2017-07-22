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

    return 0;
}

ICACHE_FLASH_ATTR int httpd_url_blank(HttpdClient *client) {
    HTTPD_IGNORE_POSTDATA

    HTTPD_OUTBUF_APPEND("HTTP/1.1 200 OK\r\n")
    HTTPD_OUTBUF_APPEND("Connection: close\r\n")
    HTTPD_OUTBUF_APPEND("Content-type: text/html\r\n")
    HTTPD_OUTBUF_APPEND("Content-length: 26\r\n")
    HTTPD_OUTBUF_APPEND("\r\n")
    HTTPD_OUTBUF_APPEND("<html><body></body></html>")

    return 0;
}

const HttpdUrl httpd_urls[] = {
    { "/",     httpd_url_blank },
    { "/fota", httpd_url_fota },
};

const size_t httpd_urlcount = sizeof(httpd_urls) / sizeof(*httpd_urls);
