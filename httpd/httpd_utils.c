#include "esp-missing-decls.h"

#include <c_types.h>
#include <osapi.h>

#include <stddef.h>

int append(char **dest, size_t *destlen, const char *src) {
    size_t srclen = os_strlen(src);

    /* Don't assume that dest is already null terminated */
    if (srclen + 1 > *destlen)
        return 1;

    /* NB This writes a null byte to *dest */
    os_strcpy(*dest, src);

    /* Allow the next call to overwrite the null byte */
    *dest    += srclen;
    *destlen -= srclen;

    return 0;
}

int querystring(const char **buf, size_t *len, const char **key, size_t *keylen,
                                               const char **val, size_t *vallen)
{
    const char *p = *buf;
    size_t n = *len;

    if (n == 0 || *p == 0)
        return 2;

    *key = p;
    for (; *p!='=' && n>0; p++, n--) ;

    if (n == 0) {
        os_printf("querystring: incomplete query string\n");
        return 1;
    }

    *keylen = p - *key;
    if (*keylen == 0) {
        os_printf("querystring: invalid query string\n");
        return 1;
    }

    /* Consume the equal character */
    p++;
    n--;

    *val = p;
    for (; *p!='&' && *p!=0 && n>0; p++, n--) ;
    *vallen = p - *val;
    /* A vallen of zero is valid */

    if (*p == '&') {
        p++;
        n--;
    }

    if (n==0 && *p!=0) {
        os_printf("querystring: not null-terminated\n");
        return 1;
    }

    *len -= p - *buf;
    *buf  = p;

    return 0;
}
