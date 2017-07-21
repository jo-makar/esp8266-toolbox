#ifndef HTTPD_PRIVATE_H
#define HTTPD_PRIVATE_H

#include <sys/types.h>

#include "../config.h"

typedef struct {
    uint8_t inuse;

    struct espconn *conn;

    #define STATE_HEADERS 0
    #define STATE_POSTDATA 1
    #define STATE_RESPONSE 2
    uint8_t state;

    #define METHOD_GET 0
    #define METHOD_POST 1
    uint8_t method;

    uint8_t url[256];
    uint8_t host[64];

    uint8_t buf[1024];
    uint8_t bufused;

    uint8_t post[1024];
    uint16_t postlen;
} HttpdClient;

extern HttpdClient httpd_clients[MAX_CONN_INBOUND];

int httpd_process(HttpdClient *client);

#endif
