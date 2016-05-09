#ifndef STRING_H
#define STRING_H

#include <stddef.h>

int atoi(const char *nptr);

char *index(const char *s, int c);

size_t strnlen(const char *s, size_t maxlen);

const char *strstrn(const char *haystack, size_t n, const char *needle);

#endif
