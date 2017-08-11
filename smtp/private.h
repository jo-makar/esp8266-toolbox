#ifndef SMTP_PRIVATE_H
#define SMTP_PRIVATE_H

#include <ip_addr.h>

typedef struct {
    char host[64];
    ip_addr_t ip;
    uint16_t port;
    char user[64];
    char pass[64];
} SmtpServer;

extern SmtpServer smtp_server;

#endif
