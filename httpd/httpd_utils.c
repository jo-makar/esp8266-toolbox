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
