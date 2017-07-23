#include <ip_addr.h> /* Must be included before espconn.h */

#include <sys/param.h>
#include <espconn.h>
#include <osapi.h>
#include <stdlib.h>

#include "config.h"
#include "httpd.h"
#include "missing-decls.h"

/*
 * Parse a line from buf (of len chars) where lines are terminated with "\r\n".
 * If succesful return a pointer to the char after the line, otherwise NULL.
 */
uint8_t *parseline(const uint8_t *buf, size_t len);

/*
 * Parse a token from buf (of len chars) where tokens are whitespace delimited.
 * If successful return a pointer to char starting the token, otherwise NULL.
 * Also if end is not NULL also sets it to the char after the token.
 *
 * parsetoken(buf="foo bar")  => rv=buf,   *end=buf+3
 * parsetoken(buf=" foo bar") => rv=buf+1, *end=buf+4
 * parsetoken(buf="   ")      => rv=NULL
 * parsetoken(buf="foo")      => rv=buf,   *end=buf+3
 */
uint8_t *parsetoken(const uint8_t *buf, size_t len, uint8_t **end);

ICACHE_FLASH_ATTR int httpd_process(HttpdClient *client) {
    if (client->state == HTTPD_STATE_HEADERS) {
        uint8_t *buf = client->buf;
        int len = client->bufused;

        uint8_t *nextline;
        uint8_t *token, *nexttoken;

        /*
         * Process the request line
         * Eg "GET /foo/bar HTTP/1.1\r\n"
         */

        if ((nextline = parseline(buf, len)) == NULL)
            return 0;

        if ((token = parsetoken(buf, nextline-buf, &nexttoken)) == NULL) {
            HTTPD_WARNING("process: incomplete request line\n")
            return 1;
        }

        if (os_strncmp((char *)token, "GET", MIN(nexttoken-token, 3)) == 0)
            client->method = HTTPD_METHOD_GET;
        else if (os_strncmp((char *)token, "POST", MIN(nexttoken-token, 4)) == 0)
            client->method = HTTPD_METHOD_POST;
        else {
            nexttoken = 0;
            HTTPD_WARNING("process: unsupported method: %s\n", token)
            return 1;
        }

        if ((token = parsetoken(nexttoken,
                                nextline-nexttoken, &nexttoken)) == NULL) {
            HTTPD_WARNING("process: incomplete request line\n")
            return 1;
        }

        if ((size_t)(nexttoken-token) > sizeof(client->url)) {
            HTTPD_WARNING("process: client url overflow\n")
            return 1;
        }

        os_memcpy(client->url, token, nexttoken-token);
        client->url[nexttoken-token] = 0;

        /* For now ignore the last token; http-version field */

        /*
         * Process the header lines
         * Eg "Host: 192.168.4.1\r\n", "Content-Length: 123\r\n", etc
         */

        buf = nextline;
        len -= nextline-buf;

        while (1) {
            if (len>=2 && buf[0]=='\r' && buf[1]=='\n') {
                buf += 2;
                len -= 2;
                break;
            }

            if ((nextline = parseline(buf, len)) == NULL)
                return 0;

            if ((token = parsetoken(buf, nextline-buf, &nexttoken)) == NULL) {
                HTTPD_WARNING("process: bad header line\n")
                return 1;
            }

            if (os_strncmp((char *)token, "Host:",
                           MIN(nexttoken-token, 5)) == 0) {
                if ((token = parsetoken(nexttoken, nextline-nexttoken,
                                        &nexttoken)) == NULL) {
                    HTTPD_WARNING("process: incomplete host header line\n")
                    return 1;
                }

                if ((size_t)(nexttoken-token) > sizeof(client->host)) {
                    HTTPD_WARNING("process: client host overflow\n")
                    return 1;
                }

                os_memcpy(client->host, token, nexttoken-token);
                client->host[nexttoken-token] = 0;
            }

            else if (os_strncmp((char *)token, "Content-Length:",
                                  MIN(nexttoken-token, 15)) == 0) {
                if ((token = parsetoken(nexttoken, nextline-nexttoken,
                                        &nexttoken)) == NULL) {
                    HTTPD_WARNING("process: incomplete cl header line\n")
                    return 1;
                }

                client->postlen = atoi((char *)token);
            }

            buf = nextline;
            len -= nextline-buf;
        }

        /* Consume the processed header */
        os_memmove(client->buf, buf, client->bufused - (buf-client->buf));
        client->bufused -= buf-client->buf;

        #if HTTPD_LOG_LEVEL <= LEVEL_INFO
        {
            uint32_t t = system_get_time();

            os_printf("%u.%03u: info: %s:%u: ",
                      t/1000000, (t%1000000)/1000, __FILE__, __LINE__);
            os_printf("process: " IPSTR ":%u ",
                      IP2STR(client->conn->proto.tcp->remote_ip),
                      client->conn->proto.tcp->remote_port);

            if (client->method == HTTPD_METHOD_GET)
                os_printf("get ");
            else /* if HTTPD_METHOD_POST */
                os_printf("post len=%u ", client->postlen);

            os_printf("url=%s\n", client->url);
        }
        #endif

        if (client->method == HTTPD_METHOD_GET) {
            client->state = HTTPD_STATE_RESPONSE;
            if (client->bufused > 0)
                HTTPD_WARNING("process: extra bytes after get\n")
        } else /* if HTTPD_METHOD_POST */
            client->state = HTTPD_STATE_POSTDATA;
    }

    if (client->state == HTTPD_STATE_POSTDATA ||
            client->state == HTTPD_STATE_RESPONSE) {

        char baseurl[HTTPD_URL_LEN/2];
        char *beg=(char *)client->url, *end;
        size_t i;

        if ((end = index(beg, '?')) == NULL)
            end = beg + os_strlen(beg);

        if ((size_t)(end-beg) > sizeof(baseurl)) {
            HTTPD_WARNING("process: baseurl overflow\n")
            return 1;
        }

        os_strncpy(baseurl, beg, end-beg);
        baseurl[end-beg] = 0;

        os_bzero(httpd_outbuf, sizeof(httpd_outbuf));
        httpd_outbuflen = 0;

        for (i=0; i<httpd_urlcount; i++)
            if (os_strcmp((char *)httpd_urls[i].baseurl, baseurl) == 0)
                return httpd_urls[i].handler(client);

        return httpd_url_404(client);
    }

    return 0;
}

ICACHE_FLASH_ATTR uint8_t *parseline(const uint8_t *buf, size_t len) {
    size_t i;

    for (i=1; i<len; i++)
        if (buf[i-1]=='\r' && buf[i]== '\n')
            return (uint8_t *)buf+i+1;
    return NULL;
}

ICACHE_FLASH_ATTR uint8_t *parsetoken(const uint8_t *buf,
                                       size_t len, uint8_t **end) {
    size_t i;
    const uint8_t *start;

    /* Consume any delimiting chars */
    for (i=0; buf[i]==' ' || buf[i]=='\r' || buf[i]=='\n'; i++)
        if (i == len)
            return NULL;

    start = buf + i;

    for (i++; !(buf[i]==' ' || buf[i]=='\r' || buf[i]=='\n'); i++)
        if (i == len)
            break;

    if (end != NULL)
        *end = (uint8_t *)buf + i;
    return (uint8_t *)start;
}
