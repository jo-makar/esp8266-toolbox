#include "../log/log.h"
#include "private.h"

#include <ip_addr.h>
#include <espconn.h>
#include <osapi.h>

ICACHE_FLASH_ATTR int http_url_wifi_setup(HttpClient *client) {
    size_t beg, end;
    char *key, *val, *next;
    struct station_config conf;

    HTTP_IGNORE_POSTDATA

    os_bzero(&conf, sizeof(conf));

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
            /* FIXME STOPPED Convert url encoded chars */
        } else
            next = NULL;

        DEBUG(HTTP, "key:%s val:%s", key, val)

        if (os_strncmp(key, "ssid", 4) == 0) {
            if (os_strlen(val) >= sizeof(conf.ssid)) {
                ERROR(HTTP, "ssid overflow")
                goto fail;
            }
            os_strncpy((char *)conf.ssid, val, os_strlen(val)+1);
        }
        else if (os_strncmp(key, "pass", 4) == 0) {
            if (os_strlen(val) >= sizeof(conf.password)) {
                ERROR(HTTP, "pass overflow")
                goto fail;
            }
            os_strncpy((char *)conf.password, val, os_strlen(val)+1);
        }

        if (next == NULL)
            break;
        key = next;
    }

    if (os_strlen((char *)conf.ssid) == 0 || os_strlen((char *)conf.password) == 0)
        goto form;

    if (!wifi_station_set_config(&conf)) {
        ERROR(HTTP, "wifi_station_set_config failed");
        goto fail;
    }

    wifi_station_disconnect();
    wifi_station_connect();

    HTTP_OUTBUF_APPEND("HTTP/1.1 202 Accepted\r\n")
    HTTP_OUTBUF_APPEND("Connection: close\r\n")
    HTTP_OUTBUF_APPEND("Content-type: text/html\r\n")
    HTTP_OUTBUF_APPEND("Content-length: 48\r\n")
    HTTP_OUTBUF_APPEND("\r\n")
    HTTP_OUTBUF_APPEND("<html><body><h1>202 Accepted</h1></body></html>\n")

    if (espconn_send(client->conn, http_outbuf, http_outbuflen))
        ERROR(HTTP, "espconn_send() failed")

    return 1;

    form:

    HTTP_OUTBUF_APPEND("HTTP/1.1 200 OK\r\n")
    HTTP_OUTBUF_APPEND("Connection: close\r\n")
    HTTP_OUTBUF_APPEND("Content-type: text/html\r\n")

    #if HTTP_OUTBUF_MAXLEN > 9999
        #error "Content-length template requires adjustment"
    #endif

    HTTP_OUTBUF_APPEND("Content-length: xxxx\r\n")
    HTTP_OUTBUF_APPEND("\r\n")

    beg = http_outbuflen;

    HTTP_OUTBUF_APPEND("<html><head><style>\n")
    HTTP_OUTBUF_APPEND(".label {text-align: right}\n")
    HTTP_OUTBUF_APPEND("</style></head><body>\n")
    HTTP_OUTBUF_APPEND("<form action=\"/wifi/setup\" method=\"get\">\n")
    HTTP_OUTBUF_APPEND("<table>\n")

    HTTP_OUTBUF_APPEND("<tr>")
    HTTP_OUTBUF_PRINTF("<td class=\"label\">SSID (max %u bytes)</td>\n", sizeof(conf.ssid))
    HTTP_OUTBUF_APPEND("<td><input type=\"text\" name=\"ssid\"></td>\n")
    HTTP_OUTBUF_APPEND("<td/></tr>\n")

    HTTP_OUTBUF_APPEND("<tr>")
    HTTP_OUTBUF_PRINTF("<td class=\"label\">Password (max %u bytes)</td>\n", sizeof(conf.password))
    HTTP_OUTBUF_APPEND("<td><input type=\"text\" name=\"pass\"></td>\n")
    HTTP_OUTBUF_APPEND("<td><input type=\"submit\"/></td></tr>\n")

    HTTP_OUTBUF_APPEND("</table></form></body></html>\n")

    end = http_outbuflen;

    os_snprintf((char *)http_outbuf+77, 5, "%4u", end-beg);
    http_outbuf[81] = '\r';

    if (espconn_send(client->conn, http_outbuf, http_outbuflen))
        ERROR(HTTP, "espconn_send() failed")
    
    return 1;

    fail:

    HTTP_OUTBUF_APPEND("HTTP/1.1 400 Bad Request\r\n")
    HTTP_OUTBUF_APPEND("Connection: close\r\n")
    HTTP_OUTBUF_APPEND("Content-type: text/html\r\n")
    HTTP_OUTBUF_APPEND("Content-length: 51\r\n")
    HTTP_OUTBUF_APPEND("\r\n")
    HTTP_OUTBUF_APPEND("<html><body><h1>400 Bad Request</h1></body></html>\n")

    if (espconn_send(client->conn, http_outbuf, http_outbuflen))
        ERROR(HTTP, "espconn_send() failed")

    return 1;
}
