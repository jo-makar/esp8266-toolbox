#include "ctype.h"
#include "esp-missing-decls.h"
#include "httpd_process.h"
#include "httpd_urls.h"
#include "string.h"

#include <osapi.h>

/* These functions update the buf and len arguments as chars are consumed */
int requestline(const char **buf, size_t *len,
                uint8_t *method, const char **url, size_t *urllen);
int  headerline(const char **buf, size_t *len,
                const char **key, size_t *keylen, const char **val, size_t *vallen);

int linelen(const char *buf, size_t len);

#define MIN(a, b) (a < b ? a : b)

void httpd_process(struct espconn *conn, HttpClient *client) {
    if (client->status == HTTPCLIENT_RECV_HEADERS) {
        const char *buf = client->buf;
        size_t      len = client->bufused;

        uint8_t method;
        const char *url;
        size_t urllen;

        const char *key, *val;
        size_t keylen, vallen;

        int rv;

        /* Process the request line */

        if ((rv = requestline(&buf, &len, &method, &url, &urllen)) == 1)
            return;
        else if (rv > 1) {
            if (espconn_disconnect(conn))
                os_printf("httpd_process: espconn_disconnect failed\n");
            return;
        }

        client->method = method;

        if (urllen >= sizeof(client->url)) {
            os_printf("httpd_process: insufficient url buffer length\n");
            if (espconn_disconnect(conn))
                os_printf("httpd_process: espconn_disconnect failed\n");
            return;
        }

        os_strncpy(client->url, url, urllen);
        client->url[urllen] = 0;

        /* Loop through (and selectively copy) the header lines */

        #define COPY_HEADER(name, dest) \
            if (os_strncmp(key, name, MIN(os_strlen(name), keylen)) == 0) { \
                if (vallen >= sizeof(client->dest)) { \
                    os_printf("httpd_process: insufficient " #dest " buffer length\n"); \
                    if (espconn_disconnect(conn)) \
                        os_printf("httpd_process: espconn_disconnect failed\n"); \
                    return; \
                } \
                \
                os_strncpy(client->dest, val, vallen); \
                client->dest[vallen] = 0; \
            }

        while (1) {
            if ((rv = headerline(&buf, &len, &key, &keylen, &val, &vallen)) == 1)
                return;
            else if (rv == 2) /* End of headers */
                break;
            else if (rv == 3) { /* Invalid header lined */
                if (espconn_disconnect(conn))
                    os_printf("httpd_process: espconn_disconnect failed\n");
                return;
            }

            COPY_HEADER("Host", host)
        }

        #undef COPY_HEADER

        /* Consume the header from the client buffer */
        os_memmove(client->buf, buf, len);
        client->bufused = len;

        client->status = client->method == HTTPCLIENT_POST ?
                             HTTPCLIENT_RECV_POSTDATA : HTTPCLIENT_SEND_RESPONSE;

        os_printf("httpd_process: %s %s %s from " IPSTR ":%u\n",
                  client->method == HTTPCLIENT_GET ? "GET" : "POST",
                  client->url, client->host,
                  IP2STR(client->ip), client->port);

        /* Check for and hand off to a matching url */

        {
            char baseurl[128];
            char *beg, *end;

            size_t i;

            beg = client->url;
            if ((end = index(client->url, '?')) == NULL)
                end = client->url + urllen;

            if ((size_t)(end - beg) >= sizeof(baseurl)) {
                os_printf("httpd_process: insufficient base url buffer length\n");
                if (espconn_disconnect(conn))
                    os_printf("httpd_process: espconn_disconnect failed\n");
                return;
            }

            /* beg (client->url) is null terminated */
            os_strncpy(baseurl, beg, end-beg);
            baseurl[end-beg] = 0;

            for (i=0; urls[i].handler != NULL; i++)
                /* Both urls[i].baseurl and baseurl are null terminated */
                if (os_strcmp(urls[i].baseurl, baseurl) == 0) { 
                    urls[i].handler(conn, client);
                    return;
                }

            url_404(conn, client);
            return;
        }
    }
}

int requestline(const char **buf, size_t *len,
                uint8_t *method, const char **url, size_t *urllen)
{
    const char *p;
    const char *s, *e;
    int n;

    p = *buf;
    if ((n = linelen(*buf, *len)) == -1)
        return 1;

    /* Identify the http method */
    if (os_strncmp(p, "GET ", MIN(4, n)) == 0) {
        *method = HTTPCLIENT_GET;
        p += 4;
        n -= 4;
    } else if (os_strncmp(p, "POST ", MIN(5, n)) == 0) {
        *method = HTTPCLIENT_POST;
        p += 5;
        n -= 5;
    } else {
        os_printf("httpd_process: requestline: unsupported method\n");
        return 2;
    }

    /* Consume any space characters */
    for (; *p==' ' && n>0; p++, n--) ;

    if (n == 0) {
        os_printf("httpd_process: requestline: incomplete request line\n");
        return 1;
    }

    /* Identify the start and end of the url */
    s = p;
    for (; *p!=' ' && n>0; p++, n--) ;
    e = p;

    *url    = s;
    *urllen = e - s;

    /* Consume any space characters */
    for (; *p==' ' && n>0; p++, n--) ;

    /* Verify the http-version field */

    if (os_strncmp(p, "HTTP/", MIN(5, n))) {
        os_printf("httpd_process: requestline: invalid http-version\n");
        return 2;
    }

    if (!isdigit(*(p+5))) {
        os_printf("httpd_process: requestline: invalid http-version\n");
        return 2;
    }
    if (*(p+6) != '.') {
        os_printf("httpd_process: requestline: invalid http-version\n");
        return 2;
    }
    if (!isdigit(*(p+7))) {
        os_printf("httpd_process: requestline: invalid http-version\n");
        return 2;
    }

    p += 8;
    n -= 8;

    /* Consume any space characters */
    for (; *p==' ' && n>0; p++, n--) ;

    if (n < 2) {
        os_printf("httpd_process: requestline: incomplete request line\n");
        return 1;
    }

    if (*p!='\r' || *(p+1)!='\n' || n>2) {
        os_printf("httpd_process: requestline: extra chars at end of request line\n");
        return 2;
    }

    *len -= (p+2) - *buf;
    *buf  = p+2;

    return 0;
}

int headerline(const char **buf, size_t *len, const char **key, size_t *keylen,
                                              const char **val, size_t *vallen)
{
    const char *p;
    int n;

    p = *buf;
    if ((n = linelen(*buf, *len)) == -1)
        return 1;

    /* Consume any space characters */
    for (; *p==' ' && n>0; p++, n--) ;

    if (n == 0) {
        os_printf("httpd_process: headerline: incomplete header line\n");
        return 1;
    }

    /* An empty header line indicates the end of headers */
    if (n==2 && *p=='\r' && *(p+1)=='\n') {
        *len -= (p+2) - *buf;
        *buf  = p+2;
        return 2;
    }

    *key = p;
    for (; *p!=':' && n>0; p++, n--) ;

    if (n == 0) {
        os_printf("httpd_process: headerline: incomplete header line\n");
        return 1;
    }
    *keylen = p - *key;

    /* Consume the colon character (make up your own rude joke) */
    p++;
    n--;

    /* Consume any space characters */
    for (; *p==' ' && n>0; p++, n--) ;

    if (n == 0) {
        os_printf("httpd_process: headerline: incomplete header line\n");
        return 1;
    }

    *val = p;

    if ((p = strstrn(p, n, "\r\n")) == NULL) {
        os_printf("httpd_process: headerline: incomplete header line\n");
        return 1;
    }
    *vallen = p - *val;

    n -= p - *val;
    if (n > 2) {
        os_printf("httpd_process: headerline: extra chars at end of header line\n");
        return 3;
    }

    *len -= (p+2) - *buf;
    *buf  = p+2;

    return 0;
}

int linelen(const char *buf, size_t len) {
    size_t i;

    for (i=1; i<len; i++)
        if (buf[i-1]=='\r' && buf[i]=='\n')
            return i+1;
    return -1;
}
