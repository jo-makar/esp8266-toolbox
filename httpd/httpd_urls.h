#ifndef HTTPD_URLS_H
#define HTTPD_URLS_H

#include "httpclient.h"

/* Must be included before espconn.h */
#include <ip_addr.h>

#include <espconn.h>

void url_404(struct espconn *conn, HttpClient *client);

typedef struct {
    char baseurl[128];
    void (*handler)(struct espconn *, HttpClient *);
} Url;

extern const Url urls[];

#endif
