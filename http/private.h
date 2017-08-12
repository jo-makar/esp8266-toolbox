#ifndef HTTP_PRIVATE_H
#define HTTP_PRIVATE_H

#include "../missing-decls.h"
#include "utils.h"

#include <sys/param.h>
#include <ip_addr.h>
#include <espconn.h>
#include <osapi.h>

/*
 * Client data
 */

typedef struct {
    uint8_t inuse;

    struct espconn *conn;

    #define HTTP_STATE_HEADERS 0
    #define HTTP_STATE_POSTDATA 1
    #define HTTP_STATE_RESPONSE 2
    uint8_t state;

    #define HTTP_METHOD_GET 0
    #define HTTP_METHOD_POST 1
    uint8_t method;

    #define HTTP_URL_LEN 256
    uint8_t url[HTTP_URL_LEN];
    uint8_t host[64];

    uint8_t buf[2048];
    uint16_t bufused;

    uint32_t postused;
    uint32_t postlen;
} HttpClient;

/*
 * Task handling
 */

#define HTTP_TASK_PRIO USER_TASK_PRIO_0

#define HTTP_TASK_KILL 0

/*
 * Output buffer handling
 */

#define HTTP_OUTBUF_MAXLEN 4*1024 
extern uint8_t http_outbuf[HTTP_OUTBUF_MAXLEN];
extern uint16_t http_outbuflen;

#define HTTP_IGNORE_POSTDATA { \
    if (client->state == HTTP_STATE_POSTDATA) { \
        size_t len = MIN(client->bufused, client->postlen - client->postused); \
        \
        client->postused += len; \
        \
        os_memmove(client->buf, client->buf + len, client->bufused - len); \
        client->bufused -= len; \
        \
        if (client->postused == client->postlen) { \
            client->state = HTTP_STATE_RESPONSE; \
            if (client->bufused > 0) \
                WARNING(HTTP, "extra bytes after post data") \
        } \
        else \
            return 0; \
    } \
}

#define HTTP_OUTBUF_APPEND(src) { \
    size_t srclen; \
    \
    if ((srclen = os_strlen(src)) >= sizeof(http_outbuf)-http_outbuflen) { \
        ERROR(HTTP, "http_outbuf overflow") \
        return 0; \
    } \
    \
    os_strncpy((char *)http_outbuf+http_outbuflen, src, srclen+1); \
    http_outbuflen += srclen; \
}

#define HTTP_OUTBUF_PRINTF(fmt, ...) { \
    char buf[128]; \
    size_t len; \
    \
    if ((len = os_snprintf(buf, sizeof(buf), fmt, ##__VA_ARGS__)) \
            >= sizeof(buf)) { \
        ERROR(HTTP, "HTTP_OUTBUF_PRINTF buf overflow\n") \
        return 0; \
    } \
    \
    if (len >= sizeof(http_outbuf)-http_outbuflen) { \
        ERROR(HTTP, "http_outbuf overflow\n") \
        return 0; \
    } \
    \
    os_strncpy((char *)http_outbuf+http_outbuflen, buf, len+1); \
    http_outbuflen += len; \
}

/*
 * Url processing/matching
 */

typedef struct {
    #define HTTP_BASEURL_LEN HTTP_URL_LEN/2
    uint8_t baseurl[HTTP_BASEURL_LEN];
    int (*handler)(HttpClient *);
} HttpUrl;

extern const HttpUrl http_urls[];
extern const size_t http_urlcount;

int http_process(HttpClient *client);

int http_url_404(HttpClient *client);

int http_url_blank(HttpClient *client);

int http_url_logs(HttpClient *client);
int http_url_logs_lower(HttpClient *client);
int http_url_logs_raise(HttpClient *client);

int http_url_ota_bin(HttpClient *client);
int http_url_ota_push(HttpClient *client);

int http_url_reset(HttpClient *client);

int http_url_smtp_setup(HttpClient *client);

int http_url_uptime(HttpClient *client);

int http_url_version(HttpClient *client);

int http_url_wifi_setup(HttpClient *client);

#endif
