#ifndef HTTPD_H
#define HTTPD_H

#define HTTPD_OUTBUF_MAXLEN 5*1024

#include "private.h"

void httpd_init();
void httpd_stop();

#endif
