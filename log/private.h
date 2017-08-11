#ifndef LOG_PRIVATE_H
#define LOG_PRIVATE_H

#include "../http/private.h"

typedef struct {
    char main[HTTP_OUTBUF_MAXLEN-500];
    char line[192];
    char entry[256];
} LogBuf;

extern LogBuf logbuf;

void log_entry(const char *level, const char *file, int line, const char *entry);

#endif
