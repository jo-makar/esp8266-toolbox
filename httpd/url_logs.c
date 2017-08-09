#include <ip_addr.h> /* Must be included before espconn.h */

#include <espconn.h>
#include <osapi.h>

#include "../log.h"
#include "httpd.h"

ICACHE_FLASH_ATTR int httpd_url_logs(HttpdClient *client) {
    #if !LOG_URLBUF_ENABLE
        HTTPD_IGNORE_POSTDATA

        HTTPD_OUTBUF_APPEND("HTTP/1.1 404 Not Found\r\n")
        HTTPD_OUTBUF_APPEND("Connection: close\r\n")
        HTTPD_OUTBUF_APPEND("Content-type: text/html\r\n")
        HTTPD_OUTBUF_APPEND("Content-length: 49\r\n")
        HTTPD_OUTBUF_APPEND("\r\n")
        HTTPD_OUTBUF_APPEND("<html><body><h1>404 Not Found</h1></body></html>\n")

        if (espconn_secure_send(client->conn, httpd_outbuf, httpd_outbuflen))
            LOG_ERROR(HTTPD, "url_404: espconn_secure_send() failed\n")

        return 1;
    #else
        size_t beg, end;

        HTTPD_OUTBUF_APPEND("HTTP/1.1 200 OK\r\n")
        HTTPD_OUTBUF_APPEND("Connection: close\r\n")
        HTTPD_OUTBUF_APPEND("Content-type: text/html\r\n")

        #if HTTPD_OUTBUF_MAXLEN > 9999
            #error "Content-length template requires adjustment"
        #endif

        HTTPD_OUTBUF_APPEND("Content-length: xxxx\r\n")
        HTTPD_OUTBUF_APPEND("\r\n")

        beg = httpd_outbuflen;

        HTTPD_OUTBUF_APPEND("<html><body><pre>\n")
            
        if (sizeof(_log.urlbuf) >= sizeof(httpd_outbuf)-httpd_outbuflen) {
            LOG_ERROR(HTTPD, "httpd_outbuf overflow\n")
                return 0;
        }

        os_memcpy(httpd_outbuf + httpd_outbuflen, _log.urlbuf,
                  sizeof(_log.urlbuf));
        httpd_outbuflen += sizeof(_log.urlbuf);

        HTTPD_OUTBUF_APPEND("</pre>\n")

        if (os_strstr((char *)client->url, "?refresh") != NULL) {
            HTTPD_OUTBUF_APPEND("<script type=\"text/javascript\">\n")
            HTTPD_OUTBUF_APPEND("setTimeout(function() {\n")
            HTTPD_OUTBUF_APPEND("    window.location.reload(true); }, 20000);\n")
            HTTPD_OUTBUF_APPEND("</script>\n")
        }

        HTTPD_OUTBUF_APPEND("</body></html>\n")

        end = httpd_outbuflen;

        /*
        * Write the Content-length value
        *
        * Apparently os_snprintf() behaves differently than snprintf(),
        * it os_snprintf() always writes the terminating null byte
        */
        os_snprintf((char *)httpd_outbuf+77, 5, "%4u", end-beg);
        httpd_outbuf[81] = '\r';

        if (espconn_secure_send(client->conn, httpd_outbuf, httpd_outbuflen))
            LOG_ERROR(HTTPD, "espconn_secure_send() failed\n")

        return 1;
    #endif
}
