#include "../log/log.h"
#include "../missing-decls.h"
#include "private.h"

#include <sys/param.h>
#include <stdlib.h>

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

ICACHE_FLASH_ATTR int http_process(HttpClient *client) {
    if (client->state == HTTP_STATE_HEADERS) {
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
            WARNING(HTTP, "incomplete request line")
            return 1;
        }

        if (os_strncmp((char *)token, "GET", MIN(nexttoken-token, 3)) == 0)
            client->method = HTTP_METHOD_GET;
        else if (os_strncmp((char *)token, "POST", MIN(nexttoken-token, 4)) == 0)
            client->method = HTTP_METHOD_POST;
        else {
            nexttoken = 0;
            WARNING(HTTP, "unsupported method: %s", token)
            return 1;
        }

        if ((token = parsetoken(nexttoken, nextline-nexttoken, &nexttoken)) == NULL) {
            WARNING(HTTP, "incomplete request line")
            return 1;
        }

        if ((size_t)(nexttoken-token) > sizeof(client->url)) {
            WARNING(HTTP, "client url overflow")
            return 1;
        }

        os_memcpy(client->url, token, nexttoken-token);
        client->url[nexttoken-token] = 0;

        /*
         * Process the header lines
         * Eg "Host: 192.168.4.1\r\n", "Content-Length: 123\r\n", etc
         */

        buf = nextline;
        len -= nextline-buf;

        while (1) {
            system_soft_wdt_feed();

            if (len>=2 && buf[0]=='\r' && buf[1]=='\n') {
                buf += 2;
                len -= 2;
                break;
            }

            if ((nextline = parseline(buf, len)) == NULL)
                return 0;

            if ((token = parsetoken(buf, nextline-buf, &nexttoken)) == NULL) {
                WARNING(HTTP, "bad header line")
                return 1;
            }

            if (os_strncmp((char *)token, "Host:", MIN(nexttoken-token, 5)) == 0) {
                if ((token = parsetoken(nexttoken, nextline-nexttoken, &nexttoken)) == NULL) {
                    WARNING(HTTP, "incomplete host header line")
                    return 1;
                }

                if ((size_t)(nexttoken-token) > sizeof(client->host)) {
                    WARNING(HTTP, "client host overflow")
                    return 1;
                }

                os_memcpy(client->host, token, nexttoken-token);
                client->host[nexttoken-token] = 0;
            }

            else if (os_strncmp((char *)token, "Content-Length:", MIN(nexttoken-token, 15)) == 0) {
                if ((token = parsetoken(nexttoken, nextline-nexttoken, &nexttoken)) == NULL) {
                    WARNING(HTTP, "incomplete content-length header line")
                    return 1;
                }

                client->postlen = atoi((char *)token);
            }

            buf = nextline;
            len -= nextline-buf;
        }

        os_memmove(client->buf, buf, client->bufused - (buf-client->buf));
        client->bufused -= buf-client->buf;

        if (client->method == HTTP_METHOD_POST)
            INFO(HTTP, IPSTR ":%u host=%s url=%s post=%u",
                       IP2STR(client->conn->proto.tcp->remote_ip),
                       client->conn->proto.tcp->remote_port,
                       client->host, client->url, client->postlen)
        else 
            INFO(HTTP, IPSTR ":%u host=%s url=%s",
                       IP2STR(client->conn->proto.tcp->remote_ip),
                       client->conn->proto.tcp->remote_port,
                       client->host, client->url)

        if (client->method == HTTP_METHOD_GET) {
            client->state = HTTP_STATE_RESPONSE;
            if (client->bufused > 0)
                WARNING(HTTP, "extra bytes after get")
        } else
            client->state = HTTP_STATE_POSTDATA;
    }

    if (client->state == HTTP_STATE_POSTDATA || client->state == HTTP_STATE_RESPONSE) {
        char baseurl[HTTP_BASEURL_LEN];
        char *beg=(char *)client->url, *end;
        size_t i;

        if ((end = index(beg, '?')) == NULL)
            end = beg + os_strlen(beg);

        if ((size_t)(end-beg) > sizeof(baseurl)) {
            WARNING(HTTP, "baseurl overflow\n")
            return 1;
        }

        os_strncpy(baseurl, beg, end-beg);
        baseurl[end-beg] = 0;

        os_bzero(http_outbuf, sizeof(http_outbuf));
        http_outbuflen = 0;

        for (i=0; i<http_urlcount; i++)
            if (os_strcmp((char *)http_urls[i].baseurl, baseurl) == 0)
                return http_urls[i].handler(client);

        return http_url_404(client);
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
