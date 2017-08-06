#include <ip_addr.h> /* Must be included before espconn.h */

#include <espconn.h>
#include <osapi.h>

#include "httpd.h"

ICACHE_FLASH_ATTR int httpd_url_wifi_setup(HttpdClient *client) {
    HTTPD_IGNORE_POSTDATA

    size_t beg, end;
    char *key, *val, *next;
    struct station_config conf;

    os_bzero(&conf, sizeof(conf));
    /* TODO Support setting the conf.bssid_set and conf.bssid params */

    if (index((char *)client->url, '?') == NULL)
        goto form;
    key = index((char *)client->url, '?') + 1;

    while (1) {
        if (index(key, '=') == NULL)
            goto form;
        val = index(key, '=') + 1;
        *(val - 1) = 0;

        if (index(val, '&') != NULL) {
            next = index(val, '&') + 1;
            *(next - 1) = 0;
        } else
            next = NULL;

        DEBUG(HTTPD, "key:%s val:%s\n", key, val)

        if (os_strncmp(key, "ssid", 4) == 0) {
            if (os_strlen(val) >= sizeof(conf.ssid)) {
                ERROR(HTTPD, "ssid overflow\n");
                goto fail;
            }
            os_strncpy((char *)conf.ssid, val, os_strlen(val)+1);
        }
        else if (os_strncmp(key, "pass", 4) == 0) {
            if (os_strlen(val) >= sizeof(conf.password)) {
                ERROR(HTTPD, "pass overflow\n");
                goto fail;
            }
            os_strncpy((char *)conf.password, val, os_strlen(val)+1);
        }

        if (next == NULL)
            break;
        key = next;
    }

    if (os_strlen((char *)conf.ssid) == 0 ||
            os_strlen((char *)conf.password) == 0)
        goto form;

    if (!wifi_station_set_config(&conf)) {
        ERROR(HTTPD, "wifi_station_set_config failed\n");
        goto fail;
    }

    wifi_station_disconnect();
    wifi_station_connect();

    HTTPD_OUTBUF_APPEND("HTTP/1.1 202 Accepted\r\n")
    HTTPD_OUTBUF_APPEND("Connection: close\r\n")
    HTTPD_OUTBUF_APPEND("Content-type: text/html\r\n")
    HTTPD_OUTBUF_APPEND("Content-length: 48\r\n")
    HTTPD_OUTBUF_APPEND("\r\n")
    HTTPD_OUTBUF_APPEND("<html><body><h1>202 Accepted</h1></body></html>\n")

    if (espconn_send(client->conn, httpd_outbuf, httpd_outbuflen))
        ERROR(HTTPD, "espconn_send() failed\n")

    return 1;

    form:

    HTTPD_OUTBUF_APPEND("HTTP/1.1 200 OK\r\n")
    HTTPD_OUTBUF_APPEND("Connection: close\r\n")
    HTTPD_OUTBUF_APPEND("Content-type: text/html\r\n")

    #if HTTPD_OUTBUF_MAXLEN > 9999
        #error "Content-length template requires adjustment"
    #endif

    HTTPD_OUTBUF_APPEND("Content-length: xxxx\r\n")
    HTTPD_OUTBUF_APPEND("\r\n")

    beg = httpd_outbuflen;

    HTTPD_OUTBUF_APPEND("<html><head><style>\n")
    HTTPD_OUTBUF_APPEND(".label {text-align: right}\n")
    HTTPD_OUTBUF_APPEND("</style></head><body>\n")
    HTTPD_OUTBUF_APPEND("<form action=\"/wifi/setup\" method=\"get\">\n")
    HTTPD_OUTBUF_APPEND("<table>\n")

    HTTPD_OUTBUF_APPEND("<tr>")
    HTTPD_OUTBUF_PRINTF("<td class=\"label\">SSID (max %u bytes)</td>\n",
                        sizeof(conf.ssid))
    HTTPD_OUTBUF_APPEND("<td><input type=\"text\" name=\"ssid\"></td>\n")
    HTTPD_OUTBUF_APPEND("<td/></tr>\n")

    HTTPD_OUTBUF_APPEND("<tr>")
    HTTPD_OUTBUF_PRINTF("<td class=\"label\">Password (max %u bytes)</td>\n",
                        sizeof(conf.password))
    HTTPD_OUTBUF_APPEND("<td><input type=\"text\" name=\"pass\"></td>\n")
    HTTPD_OUTBUF_APPEND("<td><input type=\"submit\"/></td></tr>\n")

    HTTPD_OUTBUF_APPEND("</table></form></body></html>\n")

    end = httpd_outbuflen;

    /*
     * Write the Content-length value
     *
     * Apparently os_snprintf() behaves differently than snprintf(),
     * it os_snprintf() always writes the terminating null byte
     */
    os_snprintf((char *)httpd_outbuf+77, 5, "%4u", end-beg);
    httpd_outbuf[81] = '\r';

    if (espconn_send(client->conn, httpd_outbuf, httpd_outbuflen))
        ERROR(HTTPD, "espconn_send() failed\n")
    
    return 1;

    fail:

    HTTPD_OUTBUF_APPEND("HTTP/1.1 400 Bad Request\r\n")
    HTTPD_OUTBUF_APPEND("Connection: close\r\n")
    HTTPD_OUTBUF_APPEND("Content-type: text/html\r\n")
    HTTPD_OUTBUF_APPEND("Content-length: 51\r\n")
    HTTPD_OUTBUF_APPEND("\r\n")
    HTTPD_OUTBUF_APPEND("<html><body><h1>400 Bad Request</h1></body></html>\n")

    if (espconn_send(client->conn, httpd_outbuf, httpd_outbuflen))
        ERROR(HTTPD, "espconn_send() failed\n")

    return 1;
}
