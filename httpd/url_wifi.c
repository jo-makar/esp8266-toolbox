#include <ip_addr.h> /* Must be included before espconn.h */

#include <espconn.h>

#include "httpd.h"

ICACHE_FLASH_ATTR int httpd_url_wifi_setup(HttpdClient *client) {
    HTTPD_IGNORE_POSTDATA

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

    HTTPD_OUTBUF_APPEND("<html><head><style>\n")
    HTTPD_OUTBUF_APPEND(".label {text-align: right}\n")
    HTTPD_OUTBUF_APPEND("</style></head><body>\n")
    HTTPD_OUTBUF_APPEND("<form action=\"/wifi/setup\" method=\"get\">\n")
    HTTPD_OUTBUF_APPEND("<table>\n")

    HTTPD_OUTBUF_APPEND("<tr><td class=\"label\">SSID</td>\n")
    HTTPD_OUTBUF_APPEND("<td><input type=\"text\" name=\"label\"></td>\n")
    HTTPD_OUTBUF_APPEND("<td/></tr>\n")

    HTTPD_OUTBUF_APPEND("<tr><td class=\"label\">Password</td>\n")
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
}
