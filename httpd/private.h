#ifndef HTTPD_PRIVATE_H
#define HTTPD_PRIVATE_H

#include <sys/param.h>
#include <sys/types.h>
#include <osapi.h>

#include "../config.h"
#include "../log.h"
#include "../missing-decls.h"

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

    uint8_t buf[2048];
    uint16_t bufused;

    uint32_t postused;
    uint32_t postlen;
} HttpdClient;

extern HttpdClient httpd_clients[HTTPD_MAX_CONN];

enum httpd_task_signal { HTTPD_DISCONN };

typedef struct {
    uint8_t baseurl[HTTPD_URL_LEN/2];
    int (*handler)(HttpdClient *);
} HttpdUrl;

extern const HttpdUrl httpd_urls[];
extern const size_t httpd_urlcount;

uint8_t httpd_outbuf[HTTPD_OUTBUF_MAXLEN];
uint16_t httpd_outbuflen;

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
                LOG_WARNING(HTTPD, "extra bytes after post data\n") \
        } \
        else \
            return 0; \
    } \
}

#define HTTPD_OUTBUF_APPEND(src) { \
    size_t srclen; \
    \
    if ((srclen = os_strlen(src)) >= sizeof(httpd_outbuf)-httpd_outbuflen) { \
        LOG_ERROR(HTTPD, "httpd_outbuf overflow\n") \
        return 0; \
    } \
    \
    /* Copy srclen+1 for the terminating null byte */ \
    os_strncpy((char *)httpd_outbuf+httpd_outbuflen, src, srclen+1); \
    httpd_outbuflen += srclen; \
}

#define HTTPD_OUTBUF_PRINTF(fmt, ...) { \
    char buf[128]; \
    size_t len; \
    \
    if ((len = os_snprintf(buf, sizeof(buf), fmt, ##__VA_ARGS__)) \
            >= sizeof(buf)) { \
        LOG_ERROR(HTTPD, "HTTPD_OUTBUF_PRINTF buf overflow\n") \
        return 0; \
    } \
    \
    if (len >= sizeof(httpd_outbuf)-httpd_outbuflen) { \
        LOG_ERROR(HTTPD, "httpd_outbuf overflow\n") \
        return 0; \
    } \
    \
    /* Copy len+1 for the terminating null byte */ \
    os_strncpy((char *)httpd_outbuf+httpd_outbuflen, buf, len+1); \
    httpd_outbuflen += len; \
}

int httpd_process(HttpdClient *client);

int httpd_url_404(HttpdClient *client);

int httpd_url_fota_bin(HttpdClient *client);
int httpd_url_fota_push(HttpdClient *client);

int httpd_url_version(HttpdClient *client);
int httpd_url_uptime(HttpdClient *client);

int httpd_url_wifi_setup(HttpdClient *client);

int httpd_url_logs(HttpdClient *client);

int httpd_url_reset(HttpdClient *client);

#endif
