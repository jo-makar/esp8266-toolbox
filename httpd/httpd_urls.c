#include "esp-missing-decls.h"
#include "httpd_urls.h"
#include "httpd_utils.h"

#include <osapi.h>
#include <user_interface.h>

#include <stddef.h>

/*
 * For a long-running process post a message to a dedicated task,
 * questionable if this should be done as part of this architecture by baseurl.
 */

void url_blank(struct espconn *conn, HttpClient *client);
 void url_wifi(struct espconn *conn, HttpClient *client);

const Url urls[] = {
    { "/wifi", url_wifi },
    { "/",     url_blank },
    { "",      NULL },
};

void url_404(struct espconn *conn, HttpClient *client) {
    (void)client;

    char  out[256];
    char  *buf = out;
    size_t len = sizeof(out);

    APPEND(buf, len, "HTTP/1.1 404 Not Found\r\n")
    APPEND(buf, len, "Connection: close\r\n")
    APPEND(buf, len, "Content-type: text/html\r\n")
    APPEND(buf, len, "Content-length: 48\r\n")
    APPEND(buf, len, "\r\n")
    APPEND(buf, len, "<html><body><h1>404 Not Found</h1></body></html>")

    if (espconn_send(conn, (uint8_t *)out, sizeof(out)-len))
        os_printf("url_404: espconn_send failed\n");
    if (espconn_disconnect(conn))
        os_printf("url_404: espconn_disconnect failed\n");
}

void url_blank(struct espconn *conn, HttpClient *client) {
    (void)client;

    #define HEADER_MAXLEN 256
    #define BODY_MAXLEN   1024

    char buf[HEADER_MAXLEN + BODY_MAXLEN];

    char  *headerbuf = buf;
    size_t headerlen = HEADER_MAXLEN;       /* Header (buffer) length (available) */

    char  *bodybuf = buf + HEADER_MAXLEN;
    size_t bodylen = BODY_MAXLEN;           /* Body (buffer) length (available) */

    size_t headerlenused, bodylenused;      /* Header, body (buffer) length used */

    char line[256];

    /********************************************/

    APPEND(bodybuf, bodylen, "<html><body></body></html>\n")

    /********************************************/

    APPEND(headerbuf, headerlen, "HTTP/1.1 200 OK\r\n")
    APPEND(headerbuf, headerlen, "Connection: close\r\n")
    APPEND(headerbuf, headerlen, "Content-type: text/html\r\n")

    APRINTF(headerbuf, headerlen, line, "Content-length: %u\r\n", BODY_MAXLEN - bodylen)

    APPEND(headerbuf, headerlen, "\r\n")

    /********************************************/

    /* Apparently consecutive sends (of header and body) does not work with the ESP SDK */

    headerlenused = HEADER_MAXLEN - headerlen;
      bodylenused =   BODY_MAXLEN -   bodylen;

    os_memmove(buf + headerlenused, buf + HEADER_MAXLEN, bodylenused);
    buf[headerlenused + bodylenused] = 0;

    if (espconn_send(conn, (uint8_t *)buf, headerlenused + bodylenused))
        os_printf("url_blank: espconn_send failed\n");

    if (espconn_disconnect(conn))
        os_printf("url_blank: espconn_disconnect failed\n");
}

void url_wifi(struct espconn *conn, HttpClient *client) {
    (void)client;

    #define HEADER_MAXLEN 256
    #define BODY_MAXLEN   1024

    char buf[HEADER_MAXLEN + BODY_MAXLEN];

    char  *headerbuf = buf;
    size_t headerlen = HEADER_MAXLEN;       /* Header (buffer) length (available) */

    char  *bodybuf = buf + HEADER_MAXLEN;
    size_t bodylen = BODY_MAXLEN;           /* Body (buffer) length (available) */

    size_t headerlenused, bodylenused;      /* Header, body (buffer) length used */

    char line[256];

    /********************************************/

    if (client->method == HTTPCLIENT_GET) {
        APPEND(bodybuf, bodylen, "<html>\n")
        APPEND(bodybuf, bodylen, "<head><style>.label { text-align: right; }</style></head>\n")
        APPEND(bodybuf, bodylen, "<body><form action='/wifi' method='post'><table>\n")
        APPEND(bodybuf, bodylen, "<tr><td class='label'>SSID</td><td><input type='text' name='ssid'/></td></tr>\n")
        APPEND(bodybuf, bodylen, "<tr><td class='label'>Password</td><td><input type='text' name='pass'/></td></tr>\n")
        APPEND(bodybuf, bodylen, "<tr><td/><td><input type='submit'/></td></tr>\n")
        APPEND(bodybuf, bodylen, "</table></form></body></html>\n")

        /********************************************/

        APPEND(headerbuf, headerlen, "HTTP/1.1 200 OK\r\n")
        APPEND(headerbuf, headerlen, "Connection: close\r\n")
        APPEND(headerbuf, headerlen, "Content-type: text/html\r\n")

        APRINTF(headerbuf, headerlen, line, "Content-length: %u\r\n", BODY_MAXLEN - bodylen)

        APPEND(headerbuf, headerlen, "\r\n")
    }

    else /* if (client->method == HTTPCLIENT_PUT) */ {
        /* FIXME STOPPED parse client->post */

        APPEND(bodybuf, bodylen, "<html><body>\n")

        APRINTF(bodybuf, bodylen, line, "%s<br/>\n", client->post)

        APPEND(bodybuf, bodylen, "</body></html>\n")
    
        /********************************************/

        APPEND(headerbuf, headerlen, "HTTP/1.1 200 OK\r\n")
        APPEND(headerbuf, headerlen, "Connection: close\r\n")
        APPEND(headerbuf, headerlen, "Content-type: text/html\r\n")

        APRINTF(headerbuf, headerlen, line, "Content-length: %u\r\n", BODY_MAXLEN - bodylen)

        APPEND(headerbuf, headerlen, "\r\n")
    }

    /********************************************/

    /* Apparently consecutive sends (of header and body) does not work with the ESP SDK */

    headerlenused = HEADER_MAXLEN - headerlen;
      bodylenused =   BODY_MAXLEN -   bodylen;

    os_memmove(buf + headerlenused, buf + HEADER_MAXLEN, bodylenused);
    buf[headerlenused + bodylenused] = 0;

    if (espconn_send(conn, (uint8_t *)buf, headerlenused + bodylenused))
        os_printf("url_wifi: espconn_send failed\n");

    if (espconn_disconnect(conn))
        os_printf("url_wifi: espconn_disconnect failed\n");

    /* FIXME Remove
    {
        struct station_config config;

        if (!wifi_set_opmode(STATION_MODE)) {
            os_printf("url_wifi: wifi_set_opmode failed\n");
            return;
        }

        os_bzero(&config, sizeof(config));
        os_strncpy((char *)config.ssid,     "ssid", sizeof(config.ssid));
        os_strncpy((char *)config.password, "pass", sizeof(config.password));
        config.bssid_set = 0;

        if (!wifi_station_set_config(&config)) {
            os_printf("url_wifi: wifi_station_set_config failed\n");
            return;
        }

        if (!wifi_station_disconnect()) {
            os_printf("url_wifi: wifi_station_disconnect failed\n");
            return;
        }

        if (!wifi_station_connect()) {
            os_printf("url_wifi: wifi_station_connect failed\n");
            return;
        }
    }
    */
}
