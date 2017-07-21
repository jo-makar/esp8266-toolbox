#ifndef HTTPD_PRIVATE_H
#define HTTPD_PRIVATE_H

#include <sys/types.h>

#include "../config.h"

typedef struct {
    uint8_t inuse;
    struct espconn *conn;

    uint8_t buf[1024];
    uint8_t bufused;
} HttpdClient;

extern HttpdClient httpd_clients[MAX_CONN_INBOUND];

void httpd_process(HttpdClient *client);

#endif