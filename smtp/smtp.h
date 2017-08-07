#ifndef SMTP_H
#define SMTP_H

#include "private.h"

void smtp_init_gmail(const char *user, const char *pass);

int smtp_send(const char *to, const char *subj, const char *body);

#endif
