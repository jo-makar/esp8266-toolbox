#ifndef HTTPD_PRIVATE_H
#define HTTPD_PRIVATE_H

#include <sys/types.h>

#include "../config.h"

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
