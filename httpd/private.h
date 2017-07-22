#ifndef HTTPD_PRIVATE_H
#define HTTPD_PRIVATE_H

#include <sys/param.h>
#include <sys/types.h>
#include <osapi.h>

#include "../config.h"
#include "missing-decls.h"

#define HTTPD_CRITICAL(s, ...) { \
    if (HTTPD_LOG_LEVEL <= LEVEL_CRITICAL) \
        os_printf("critical: %s:%d: " s, __FILE__, __LINE__, ##__VA_ARGS__); \
    while (1) os_delay_us(1000); \
}

#define HTTPD_ERROR(s, ...) { \
    if (HTTPD_LOG_LEVEL <= LEVEL_ERROR) \
        os_printf("error: %s:%d: " s, __FILE__, __LINE__, ##__VA_ARGS__); \
}

#define HTTPD_WARNING(s, ...) { \
    if (HTTPD_LOG_LEVEL <= LEVEL_WARNING) \
        os_printf("warning: %s:%d: " s, __FILE__, __LINE__, ##__VA_ARGS__); \
}

#define HTTPD_INFO(s, ...) { \
    if (HTTPD_LOG_LEVEL <= LEVEL_INFO) \
        os_printf("info: %s:%d: " s, __FILE__, __LINE__, ##__VA_ARGS__); \
}

#define HTTPD_DEBUG(s, ...) { \
    if (HTTPD_LOG_LEVEL <= LEVEL_DEBUG) \
        os_printf("debug: %s:%d: " s, __FILE__, __LINE__, ##__VA_ARGS__); \
}

typedef struct {
    uint8_t inuse;

    struct espconn *conn;

    #define HTTPD_STATE_HEADERS 0
    #define HTTPD_STATE_POSTDATA 1
    #define HTTPD_STATE_RESPONSE 2
    uint8_t state;

    #define HTTPD_METHOD_GET 0
    #define HTTPD_METHOD_POST 1
    uint8_t method;

    #define HTTPD_URL_LEN 256
    uint8_t url[HTTPD_URL_LEN];
    uint8_t host[64];

    uint8_t buf[1024];
    uint16_t bufused;

    uint8_t post[1024];
    uint16_t postused;
    uint16_t postlen;
} HttpdClient;

extern HttpdClient httpd_clients[MAX_CONN_INBOUND];

typedef struct {
    uint8_t baseurl[HTTPD_URL_LEN/2];
    int (*handler)(HttpdClient *);
} HttpdUrl;

extern const HttpdUrl httpd_urls[];
extern const size_t httpd_urlcount;

#define HTTPD_OUTBUF_MAXLEN 1024
uint8_t httpd_outbuf[HTTPD_OUTBUF_MAXLEN];
uint16_t httpd_outbuflen;

#define HTTPD_STORE_POSTDATA { \
    size_t len; \
    \
    if ((size_t)(client->postlen-client->postused) > \
            sizeof(client->post)-client->postused) { \
        HTTPD_WARNING("urls: client post overflow\n") \
        return 1; \
    } \
    \
    len = MIN(client->bufused, client->postlen - client->postused); \
    \
    os_memcpy(client->post + client->postused, client->buf, len); \
    client->postused += len; \
    \
    os_memmove(client->buf, client->buf + len, client->bufused - len); \
    client->bufused -= len; \
    \
    if (client->postused == client->postlen) { \
        client->state = HTTPD_STATE_RESPONSE; \
        if (client->bufused > 0) \
            HTTPD_WARNING("urls: extra bytes after post\n") \
    } \
}

#define HTTPD_IGNORE_POSTDATA { \
    if (client->state == HTTPD_STATE_POSTDATA) { \
        size_t len = MIN(client->bufused, client->postlen - client->postused); \
        \
        client->postused += len; \
        \
        os_memmove(client->buf, client->buf + len, client->bufused - len); \
        client->bufused -= len; \
        \
        if (client->postused == client->postlen) { \
            client->state = HTTPD_STATE_RESPONSE; \
            if (client->bufused > 0) \
                HTTPD_WARNING("urls: extra bytes after post\n") \
        } \
        else \
            return 0; \
    } \
}

#define HTTPD_OUTBUF_APPEND(src) { \
    size_t srclen; \
    \
    if ((srclen = os_strlen(src)) >= sizeof(httpd_outbuf)-httpd_outbuflen) { \
        HTTPD_ERROR("urls: outbuf overflow\n") \
        return 1; \
    } \
    \
    /* Copy srclen+1 for the terminating null byte */ \
    os_strncpy((char *)httpd_outbuf+httpd_outbuflen, src, srclen+1); \
    httpd_outbuflen += srclen; \
}

int httpd_process(HttpdClient *client);

int httpd_url_404(HttpdClient *client);

int httpd_url_fota(HttpdClient *client);

#endif
