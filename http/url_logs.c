#include <ip_addr.h>
#include <espconn.h>
#include <osapi.h>

#include "../log/log.h"
#include "private.h"

/*
 * Sending in chunks doesn't work because the SDK must perform the send before another 
 * packet can be sent, ie a sent callback executed and the next packet sent from there.
 *
 * Therefore this approach won't work and the HTTP framework would have to be revised
 * to support it.  This particular function isn't quite worth the effort currently.
 *
 *ICACHE_FLASH_ATTR int http_url_logs(HttpClient *client) {
 *    uint16_t lineslen=0, contentlen;
 *    uint8_t refresh = 0;
 *
 *    uint16_t i;
 *    char *p, t;
 *
 *    #define MAX_LINES 100
 *    uint16_t lines[MAX_LINES];
 *    uint8_t count = 0;
 *
 *    HTTP_IGNORE_POSTDATA
 *
 *    for (i=0; i<sizeof(logbuf.main);) {
 *        if ((p = strchr(logbuf.main+i, '\n')) == NULL)
 *            break;
 *
 *        lines[count++] = i;
 *        i = (p+1) - logbuf.main;
 *        lineslen += (p - (logbuf.main+i)) + 1;
 *    }
 *
 *    if (os_strstr((char *)client->url, "?refresh") != NULL)
 *        refresh = 1;
 *
 *    contentlen = 37+50*3+29 + (refresh?107:0) + count*45 + lineslen + 23;
 *
 *    HTTP_OUTBUF_APPEND("HTTP/1.1 200 OK\r\n")
 *    HTTP_OUTBUF_APPEND("Connection: close\r\n")
 *    HTTP_OUTBUF_APPEND("Content-type: text/html\r\n")
 *        HTTP_OUTBUF_PRINTF("Content-length: %u\r\n", contentlen)
 *    HTTP_OUTBUF_APPEND("\r\n")
 *
 *    HTTP_OUTBUF_APPEND(
 *        "<html><head><style type=\"text/css\">\n"
 *        "tr:nth-child(even) { background-color: #ffffff; }\n"
 *        "tr:nth-child(odd)  { background-color: #dddddd; }\n"
 *        "pre.inline         {          display: inline;  }\n"
 *        "</style></head><body>\n"
 *    )
 *
 *    if (refresh) {
 *        HTTP_OUTBUF_APPEND("<script type=\"text/javascript\">\n")
 *        HTTP_OUTBUF_APPEND("setTimeout(function() { window.location.reload(true); }, 20000);\n")
 *        HTTP_OUTBUF_APPEND("</script>\n")
 *    }
 *
 *    HTTP_OUTBUF_APPEND("<table>\n")
 *
 *    for (i=0; i<count-1; i++) {
 *        if (i>0 && i%10==0)
 *            system_soft_wdt_feed();
 *
 *        if (http_outbuflen + 19 + (lines[i+1]-lines[i]) > HTTP_OUTBUF_MAXLEN) {
 *            if (espconn_send(client->conn, http_outbuf, http_outbuflen))
 *                ERROR(HTTP, "espconn_send() failed\n")
 *            http_outbuflen = 0;
 *        }
 *
 *        t = logbuf.main[lines[i+1]];
 *        logbuf.main[lines[i+1]] = 0;
 *
 *        HTTP_OUTBUF_APPEND("<tr><td><pre class=\"inline\">")
 *        HTTP_OUTBUF_APPEND(logbuf.main + lines[i])
 *        HTTP_OUTBUF_APPEND("</pre></td></tr>\n")
 *
 *        logbuf.main[lines[i+1]] = t;
 *    }
 *
 *    HTTP_OUTBUF_APPEND("</table></body></html>\n")
 *
 *    if (espconn_send(client->conn, http_outbuf, http_outbuflen))
 *        ERROR(HTTP, "espconn_send() failed\n")
 *
 *    return 1;
 *}
 */

ICACHE_FLASH_ATTR int http_url_logs(HttpClient *client) {
    size_t beg, end;
    uint16_t i;
    char *p, t;

    #define MAX_LINES 100
    uint16_t lines[MAX_LINES];
    uint8_t count = 0;

    HTTP_IGNORE_POSTDATA

    for (i=0; i<sizeof(logbuf.main);) {
        if ((p = strchr(logbuf.main+i, '\n')) == NULL)
            break;

        lines[count++] = i;
        i = (p+1) - logbuf.main;
    }

    HTTP_OUTBUF_APPEND("HTTP/1.1 200 OK\r\n")
    HTTP_OUTBUF_APPEND("Connection: close\r\n")
    HTTP_OUTBUF_APPEND("Content-type: text/html\r\n")

    #if HTTP_OUTBUF_MAXLEN > 9999
        #error "Content-length template requires adjustment"
    #endif

    HTTP_OUTBUF_APPEND("Content-length: xxxx\r\n")
    HTTP_OUTBUF_APPEND("\r\n")

    beg = http_outbuflen;

    HTTP_OUTBUF_APPEND(
        "<html><head><style type=\"text/css\">\n"
        "tr:nth-child(even) { background-color: #ffffff; }\n"
        "tr:nth-child(odd)  { background-color: #dddddd; }\n"
        "pre.inline         {          display: inline;  }\n"
        "</style></head><body>\n"
    )

    if (os_strstr((char *)client->url, "?refresh") != NULL) {
        HTTP_OUTBUF_APPEND("<script type=\"text/javascript\">\n")
        HTTP_OUTBUF_APPEND("setTimeout(function() { window.location.reload(true); }, 20000);\n")
        HTTP_OUTBUF_APPEND("</script>\n")
    }

    HTTP_OUTBUF_APPEND("<table>\n")

    for (i=0; i<count-1; i++) {
        if (http_outbuflen + 19 + (lines[i+1]-lines[i]) > HTTP_OUTBUF_MAXLEN-23) {
            INFO(HTTP, "avoided http_outbuf overflow")
            break;
        }

        t = logbuf.main[lines[i+1]];
        logbuf.main[lines[i+1]] = 0;

        HTTP_OUTBUF_APPEND("<tr><td><pre class=\"inline\">")
        HTTP_OUTBUF_APPEND(logbuf.main + lines[i])
        HTTP_OUTBUF_APPEND("</pre></td></tr>\n")

        logbuf.main[lines[i+1]] = t;
    }

    HTTP_OUTBUF_APPEND("</table></body></html>\n")

    end = http_outbuflen;

    os_snprintf((char *)http_outbuf+77, 5, "%4u", end-beg);
    http_outbuf[81] = '\r';

    if (espconn_send(client->conn, http_outbuf, http_outbuflen))
        ERROR(HTTP, "espconn_send() failed")

    return 1;
}

ICACHE_FLASH_ATTR int http_url_logs_lower(HttpClient *client) {
    uint8_t found=1;

    HTTP_IGNORE_POSTDATA

    if (os_strstr((char *)client->url, "?crypto") != NULL)
        log_lower(&CRYPTO_LOG_LEVEL);
    else if (os_strstr((char *)client->url, "?http") != NULL)
        log_lower(&HTTP_LOG_LEVEL);
    else if (os_strstr((char *)client->url, "?main") != NULL)
        log_lower(&MAIN_LOG_LEVEL);
    else if (os_strstr((char *)client->url, "?ota") != NULL)
        log_lower(&OTA_LOG_LEVEL);
    else if (os_strstr((char *)client->url, "?smtp") != NULL)
        log_lower(&SMTP_LOG_LEVEL);
    else
        found = 0;

    if (found) {
        HTTP_OUTBUF_APPEND("HTTP/1.1 200 OK\r\n")
        HTTP_OUTBUF_APPEND("Connection: close\r\n")
        HTTP_OUTBUF_APPEND("Content-type: text/html\r\n")
        HTTP_OUTBUF_APPEND("Content-length: 42\r\n")
        HTTP_OUTBUF_APPEND("\r\n")
        HTTP_OUTBUF_APPEND("<html><body><h1>200 OK</h1></body></html>\n")
    } else {
        HTTP_OUTBUF_APPEND("HTTP/1.1 400 Bad Request\r\n")
        HTTP_OUTBUF_APPEND("Connection: close\r\n")
        HTTP_OUTBUF_APPEND("Content-type: text/html\r\n")
        HTTP_OUTBUF_APPEND("Content-length: 51\r\n")
        HTTP_OUTBUF_APPEND("\r\n")
        HTTP_OUTBUF_APPEND("<html><body><h1>400 Bad Request</h1></body></html>\n")
    }

    if (espconn_send(client->conn, http_outbuf, http_outbuflen))
        ERROR(HTTP, "espconn_send() failed")

    return 1;
}

ICACHE_FLASH_ATTR int http_url_logs_raise(HttpClient *client) {
    uint8_t found=1;

    HTTP_IGNORE_POSTDATA

    if (os_strstr((char *)client->url, "?crypto") != NULL)
        log_raise(&CRYPTO_LOG_LEVEL);
    else if (os_strstr((char *)client->url, "?http") != NULL)
        log_raise(&HTTP_LOG_LEVEL);
    else if (os_strstr((char *)client->url, "?main") != NULL)
        log_raise(&MAIN_LOG_LEVEL);
    else if (os_strstr((char *)client->url, "?ota") != NULL)
        log_raise(&OTA_LOG_LEVEL);
    else if (os_strstr((char *)client->url, "?smtp") != NULL)
        log_raise(&SMTP_LOG_LEVEL);
    else
        found = 0;

    if (found) {
        HTTP_OUTBUF_APPEND("HTTP/1.1 200 OK\r\n")
        HTTP_OUTBUF_APPEND("Connection: close\r\n")
        HTTP_OUTBUF_APPEND("Content-type: text/html\r\n")
        HTTP_OUTBUF_APPEND("Content-length: 42\r\n")
        HTTP_OUTBUF_APPEND("\r\n")
        HTTP_OUTBUF_APPEND("<html><body><h1>200 OK</h1></body></html>\n")
    } else {
        HTTP_OUTBUF_APPEND("HTTP/1.1 400 Bad Request\r\n")
        HTTP_OUTBUF_APPEND("Connection: close\r\n")
        HTTP_OUTBUF_APPEND("Content-type: text/html\r\n")
        HTTP_OUTBUF_APPEND("Content-length: 51\r\n")
        HTTP_OUTBUF_APPEND("\r\n")
        HTTP_OUTBUF_APPEND("<html><body><h1>400 Bad Request</h1></body></html>\n")
    }

    if (espconn_send(client->conn, http_outbuf, http_outbuflen))
        ERROR(HTTP, "espconn_send() failed")

    return 1;
}
