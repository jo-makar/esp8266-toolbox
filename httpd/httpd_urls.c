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

struct station_config url_wifi_config;

volatile os_timer_t url_wifi_configtimer;

void url_wifi_configfunc(void *arg) {
    (void)arg;

    os_printf("url_wifi_configfunc\n");

    if (!wifi_set_opmode(STATION_MODE)) {
        os_printf("url_wifi_configfunc: wifi_set_opmode failed\n");
        return;
    }

    if (!wifi_station_set_config(&url_wifi_config)) {
        os_printf("url_wifi_configfunc: wifi_station_set_config failed\n");
        return;
    }

    if (!wifi_station_disconnect()) {
        os_printf("url_wifi_configfunc: wifi_station_disconnect failed\n");
        return;
    }

    if (!wifi_station_connect()) {
        os_printf("url_wifi_configfunc: wifi_station_connect failed\n");
        return;
    }
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
        const char *postbuf = client->post;
        size_t      postlen = client->postlen;

        const char *key, *val;
        size_t keylen, vallen;
        int rv;

        char ssid[sizeof(url_wifi_config.ssid) + 1];
        char pass[sizeof(url_wifi_config.password) + 1];

        os_bzero(ssid, sizeof(ssid));
        os_bzero(pass, sizeof(pass));

        while (1) {
            if ((rv = querystring(&postbuf, &postlen, &key, &keylen, &val, &vallen)) == 1) {
                if (espconn_disconnect(conn))
                    os_printf("url_wifi: espconn_disconnect failed\n");
                return;
            } else if (rv == 2)
                break;

            if (os_strncmp(key, "ssid", 4) == 0) {
                if (vallen > sizeof(url_wifi_config.ssid)) {
                    os_printf("url_wifi: ssid exceeds max SDK length\n");
                    if (espconn_disconnect(conn))
                        os_printf("url_wifi: espconn_disconnect failed\n");
                    return;
                }

                os_strncpy(ssid, val, vallen);
                ssid[vallen] = 0;
            }

            else if (os_strncmp(key, "pass", 4) == 0) {
                if (vallen > sizeof(url_wifi_config.password)) {
                    os_printf("url_wifi: password exceeds max SDK length\n");
                    if (espconn_disconnect(conn))
                        os_printf("url_wifi: espconn_disconnect failed\n");
                    return;
                }

                os_strncpy(pass, val, vallen);
                pass[vallen] = 0;
            }
        }

        if (!os_strlen(ssid)) {
            os_printf("url_wifi: missing ssid parameter\n");
            if (espconn_disconnect(conn))
                os_printf("url_wifi: espconn_disconnect failed\n");
            return;
        }

        if (!os_strlen(pass)) {
            os_printf("url_wifi: missing pass parameter\n");
            if (espconn_disconnect(conn))
                os_printf("url_wifi: espconn_disconnect failed\n");
            return;
        }

        APPEND(bodybuf, bodylen, "<html><body>\n")
        APRINTF(bodybuf, bodylen, line, "Changing to station mode with ssid = %s, pass = %s<br/>\n", ssid, pass)
        APPEND(bodybuf, bodylen, "</body></html>\n")
    
        /********************************************/

        APPEND(headerbuf, headerlen, "HTTP/1.1 200 OK\r\n")
        APPEND(headerbuf, headerlen, "Connection: close\r\n")
        APPEND(headerbuf, headerlen, "Content-type: text/html\r\n")

        APRINTF(headerbuf, headerlen, line, "Content-length: %u\r\n", BODY_MAXLEN - bodylen)

        APPEND(headerbuf, headerlen, "\r\n")

        /*
         * Use a non-repeating timer to call a function to effect the network change later,
         * since changing now breaks the HTTP connection (even after the epsconn_disconnect).
         */

        os_bzero(&url_wifi_config, sizeof(url_wifi_config));
        os_strncpy((char *)url_wifi_config.ssid,     ssid, sizeof(url_wifi_config.ssid));
        os_strncpy((char *)url_wifi_config.password, pass, sizeof(url_wifi_config.password));
        url_wifi_config.bssid_set = 0;

        os_timer_disarm((os_timer_t *)&url_wifi_configtimer);
         os_timer_setfn((os_timer_t *)&url_wifi_configtimer, url_wifi_configfunc, NULL);
           os_timer_arm((os_timer_t *)&url_wifi_configtimer, 5000, 0);
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
}
