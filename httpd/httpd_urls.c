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

void url_stations(struct espconn *conn, HttpClient *client);

const Url urls[] = {
    { "/stations", url_stations },
    { "/",         url_stations },
    { "",          NULL },
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

void url_stations(struct espconn *conn, HttpClient *client) {
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

    APPEND(bodybuf, bodylen, "<html><body>\n")

    if (wifi_get_opmode() != 0x02) {
        APPEND(bodybuf, bodylen, "<h1>Not in AP mode</h1>\n")
    }

    else if (wifi_softap_dhcps_status() != DHCP_STARTED) {
        APPEND(bodybuf, bodylen, "<h1>DHCP server not started</h1>\n")
    }
    
    else {
        struct station_info *statinfo;
        
        statinfo = wifi_softap_get_station_info();

        APPEND(bodybuf, bodylen, "<table>\n")
        APPEND(bodybuf, bodylen, "<tr><th>IP</th><th>MAC</th></tr>\n")

        while (statinfo) {
            APRINTF(bodybuf, bodylen, line, "<tr><td>" IPSTR "</td><td>" MACSTR "</td></tr>\n",
                                            IP2STR(&statinfo->ip), MAC2STR(statinfo->bssid))
            statinfo = STAILQ_NEXT(statinfo, next);
        }
        wifi_softap_free_station_info();

        APPEND(bodybuf, bodylen, "</table>\n")
    }

    APPEND(bodybuf, bodylen, "</body></html>\n")

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
        os_printf("url_stations: espconn_send failed\n");

    if (espconn_disconnect(conn))
        os_printf("url_stations: espconn_disconnect failed\n");
}
