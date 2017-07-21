#include <ip_addr.h> /* Must be included before espconn.h */

#include <espconn.h>
#include <osapi.h>

#include "httpd.h"
#include "missing-decls.h"

int httpd_url_blank(HttpdClient *client);

const HttpdUrl httpd_urls[] = {
    { "/", httpd_url_blank },
};

const size_t httpd_urlcount = sizeof(httpd_urls) / sizeof(*httpd_urls);

uint8_t httpd_outbuf[HTTPD_OUTBUF_MAXLEN];
uint16_t httpd_outbuflen;

int httpd_url_404(HttpdClient *client) {
    (void)client;

    HTTPD_OUTBUF_APPEND("HTTP/1.1 404 Not Found\r\n")
    HTTPD_OUTBUF_APPEND("Connection: close\r\n")
    HTTPD_OUTBUF_APPEND("Content-type: text/html\r\n")
    HTTPD_OUTBUF_APPEND("Content-length: 48\r\n")
    HTTPD_OUTBUF_APPEND("\r\n")
    HTTPD_OUTBUF_APPEND("<html><body><h1>404 Not Found</h1></body></html>")

    return 0;
}

int httpd_url_blank(HttpdClient *client) {
    (void)client;

    HTTPD_OUTBUF_APPEND("HTTP/1.1 200 OK\r\n")
    HTTPD_OUTBUF_APPEND("Connection: close\r\n")
    HTTPD_OUTBUF_APPEND("Content-type: text/html\r\n")
    HTTPD_OUTBUF_APPEND("Content-length: 26\r\n")
    HTTPD_OUTBUF_APPEND("\r\n")
    HTTPD_OUTBUF_APPEND("<html><body></body></html>")

    return 0;
}
