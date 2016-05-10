#ifndef HTTPCLIENT_H
#define HTTPCLIENT_H

#include <stddef.h>
#include <stdint.h>

typedef struct {
    uint8_t inuse;
    uint8_t ip[4];
    uint16_t port;

    #define HTTPCLIENT_RECV_HEADERS     0
    #define HTTPCLIENT_RECV_POSTDATA    1
    #define HTTPCLIENT_SEND_RESPONSE    2
    uint8_t status;

    char buf[2*1024];
    size_t bufused;

    #define HTTPCLIENT_GET  0
    #define HTTPCLIENT_POST 1
    uint8_t method;

    char url[256];
    char host[128];

    char post[1024+256];
    size_t postlen;
} HttpClient;

#endif
