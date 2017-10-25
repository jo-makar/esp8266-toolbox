#ifndef SMTP_H
#define SMTP_H

#include <stdint.h>

void smtp_send_launch(const char *to, const char *subj, const char *body);

int8_t smtp_send_status();
void smtp_send_reset();

/*
 * Provide access to the internal body buffer.
 * This avoids having a second buffer for complex body generation externally.
 * The buffer is to be null-terminated and to not exceed SMTP_STATE_BODYLEN.
 */
char *smtp_send_bodybuf();
#define SMTP_STATE_BODYLEN 1024

#endif
