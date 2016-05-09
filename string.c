#include "ctype.h"
#include "esp-missing-decls.h"
#include <osapi.h>
#include <stddef.h>

int atoi(const char *nptr) {
    const char *p = nptr;
    char neg = 0;
    int rv = 0;

    while (isspace(*p))
        p++;

    if (*p == '+')
        p++;
    else if (*p == '-') {
        neg = 1;
        p++;
    }

    for (; isdigit(*p); p++)
        rv = rv*10 + (*p-'0');

    return rv * (neg ? -1 : 1);
}

char *index(const char *s, int c) {
    const char *p;

    for (p=s; *p!=c && *p!=0; p++)
        ;
    return *p==c ? (char *)p : NULL;
}

size_t strnlen(const char *s, size_t maxlen) {
    const char *p;

    for (p=s; *p!=0 && (size_t)(p-s)<=maxlen; p++)
        ;
    return p-s;
}

const char *strstrn(const char *haystack, size_t n, const char *needle) {
    size_t i, j;
    int match;

    for (i=0; i<n; i++) {
        if (n-i < os_strlen(needle))
            break;

        match = 1;
        for (j=0; j<os_strlen(needle); j++)
            if (haystack[i+j] != needle[j]) {
                match = 0;
                break;
            }

        if (match)
            return haystack+i;
    }

    return NULL;
}
