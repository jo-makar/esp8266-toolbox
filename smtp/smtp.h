#ifndef SMTP_H
#define SMTP_H

#include "private.h"

void smtp_init();

void smtp_send(const char *from, const char *to,
               const char *subj, const char *body);

void smtp_send_cb(const char *from, const char *to,
                  const char *subj, int (body_cb)(void *));

#endif
