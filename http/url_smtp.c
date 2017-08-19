#include "../crypto/base64.h"
#include "../log/log.h"
#include "../smtp/private.h"
#include "../param.h"
#include "private.h"

#include <ip_addr.h>
#include <espconn.h>
#include <osapi.h>
#include <stdlib.h>

static os_timer_t smtp_param_timer;
static void smtp_param_write(void *arg);

ICACHE_FLASH_ATTR int http_url_smtp_setup(HttpClient *client) {
    size_t beg, end;
    char *key, *val, *next;
    int rv;

    HTTP_IGNORE_POSTDATA

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
        url_decode(val);

        DEBUG(HTTP, "key:%s val:%s", key, val)

        if (os_strncmp(key, "host", 4) == 0) {
            if (os_strlen(val) >= sizeof(smtp_server.host)) {
                ERROR(HTTP, "host overflow")
                goto fail;
            }
            os_strncpy(smtp_server.host, val, os_strlen(val)+1);
        }
        else if (os_strncmp(key, "port", 4) == 0) {
            smtp_server.port = atoi(val);
        }
        else if (os_strncmp(key, "user", 4) == 0) {
            if (os_strlen(val) >= sizeof(smtp_server.user)) {
                ERROR(HTTP, "user overflow")
                goto fail;
            }

            if ((rv = b64_encode((uint8_t *)val, os_strlen(val),
                                 (uint8_t *)smtp_server.user, sizeof(smtp_server.user))) == -1)
                goto fail;
            smtp_server.user[rv] = 0;
        }
        else if (os_strncmp(key, "pass", 4) == 0) {
            if (os_strlen(val) >= sizeof(smtp_server.pass)) {
                ERROR(HTTP, "pass overflow")
                goto fail;
            }

            if ((rv = b64_encode((uint8_t *)val, os_strlen(val),
                                 (uint8_t *)smtp_server.pass, sizeof(smtp_server.pass))) == -1)
                goto fail;
            smtp_server.pass[rv] = 0;
        }
        if (os_strncmp(key, "from", 4) == 0) {
            if (os_strlen(val) >= sizeof(smtp_server.from)) {
                ERROR(HTTP, "from overflow")
                goto fail;
            }
            os_strncpy(smtp_server.from, val, os_strlen(val)+1);
        }
        if (os_strncmp(key, "to", 2) == 0) {
            if (os_strlen(val) >= sizeof(smtp_server.to)) {
                ERROR(HTTP, "to overflow")
                goto fail;
            }
            os_strncpy(smtp_server.to, val, os_strlen(val)+1);
        }

        if (next == NULL)
            break;
        key = next;
    }

    if (os_strlen(smtp_server.host) == 0 || smtp_server.port == 0 ||
        os_strlen(smtp_server.user) == 0 || os_strlen(smtp_server.pass) == 0 ||
        os_strlen(smtp_server.from) == 0 || os_strlen(smtp_server.to)   == 0)
        goto form;

    os_timer_disarm(&smtp_param_timer);
    os_timer_setfn(&smtp_param_timer, smtp_param_write, NULL);
    os_timer_arm(&smtp_param_timer, 3000, false);

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
    HTTP_OUTBUF_APPEND("<form action=\"/smtp/setup\" method=\"get\">\n")
    HTTP_OUTBUF_APPEND("<table>\n")

    HTTP_OUTBUF_APPEND("<tr>")
    HTTP_OUTBUF_PRINTF("<td class=\"label\">Mail from (max %u bytes)</td>\n", sizeof(smtp_server.from))
    HTTP_OUTBUF_APPEND("<td><input type=\"text\" name=\"from\"></td>\n")
    HTTP_OUTBUF_APPEND("<td/></tr>\n")

    HTTP_OUTBUF_APPEND("<tr>")
    HTTP_OUTBUF_PRINTF("<td class=\"label\">Mail to (max %u bytes)</td>\n", sizeof(smtp_server.to))
    HTTP_OUTBUF_APPEND("<td><input type=\"text\" name=\"to\"></td>\n")
    HTTP_OUTBUF_APPEND("<td/></tr>\n")

    HTTP_OUTBUF_APPEND("<tr>")
    HTTP_OUTBUF_PRINTF("<td class=\"label\">Hostname (max %u bytes)</td>\n", sizeof(smtp_server.host))
    HTTP_OUTBUF_APPEND("<td><input type=\"text\" name=\"host\"></td>\n")
    HTTP_OUTBUF_APPEND("<td/></tr>\n")

    HTTP_OUTBUF_APPEND("<tr>")
    HTTP_OUTBUF_PRINTF("<td class=\"label\">Port number</td>\n")
    HTTP_OUTBUF_APPEND("<td><input type=\"text\" name=\"port\"></td>\n")
    HTTP_OUTBUF_APPEND("<td/></tr>\n")

    HTTP_OUTBUF_APPEND("<tr>")
    HTTP_OUTBUF_PRINTF("<td class=\"label\">Username (max %u bytes)</td>\n", sizeof(smtp_server.user))
    HTTP_OUTBUF_APPEND("<td><input type=\"text\" name=\"user\"></td>\n")
    HTTP_OUTBUF_APPEND("<td/></tr>\n")

    HTTP_OUTBUF_APPEND("<tr>")
    HTTP_OUTBUF_PRINTF("<td class=\"label\">Password (max %u bytes)</td>\n", sizeof(smtp_server.pass))
    HTTP_OUTBUF_APPEND("<td><input type=\"text\" name=\"pass\"></td>\n")
    HTTP_OUTBUF_APPEND("<td><input type=\"submit\"/></td></tr>\n")

    HTTP_OUTBUF_APPEND("</table></form></body></html>\n")

    end = http_outbuflen;

    os_snprintf((char *)http_outbuf+77, 5, "%4u", end-beg);
    http_outbuf[81] = '\r';

    if (espconn_send(client->conn, http_outbuf, http_outbuflen))
        ERROR(HTTP, "espconn_send() failed\n")
    
    return 1;

    fail:

    HTTP_OUTBUF_APPEND("HTTP/1.1 400 Bad Request\r\n")
    HTTP_OUTBUF_APPEND("Connection: close\r\n")
    HTTP_OUTBUF_APPEND("Content-type: text/html\r\n")
    HTTP_OUTBUF_APPEND("Content-length: 51\r\n")
    HTTP_OUTBUF_APPEND("\r\n")
    HTTP_OUTBUF_APPEND("<html><body><h1>400 Bad Request</h1></body></html>\n")

    if (espconn_send(client->conn, http_outbuf, http_outbuflen))
        ERROR(HTTP, "espconn_send() failed\n")

    return 1;
}

ICACHE_FLASH_ATTR void smtp_param_write(void *arg) {
    (void)arg;

    INFO(HTTP, "storing smtp info")
    param_store(PARAM_SMTP_OFFSET, (uint8_t *)&smtp_server, sizeof(smtp_server));
}
