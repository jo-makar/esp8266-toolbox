#ifndef HTTPD_PROCESS_H
#define HTTPD_PROCESS_H

#include "httpd/httpclient.h"

/* Must be included before espconn.h */
#include <ip_addr.h>

#include <espconn.h>

void httpd_process(struct espconn *conn, HttpClient *client);

#endif
