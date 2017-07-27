#include <osapi.h>

#include "../log.h"
#include "bigint.h"
#include "missing-decls.h"

ICACHE_FLASH_ATTR void bigint_zero(Bigint *big) {
    big->used = 1;
    os_bzero(big->data, sizeof(big->data));
}

static int8_t hexchar(char c);

ICACHE_FLASH_ATTR int bigint_fromhex(Bigint *big, const char *str) {
    size_t len = os_strlen(str);
    int i;
    int8_t h, l;

    big->used = 0;
    os_bzero(big->data, sizeof(big->data));

    for (i=len-1; i>=1; i-=2) {
        if ((l = hexchar(str[i])) == -1) {
            WARNING(MAIN, "invalid hex char\n")
            return 1;
        }
        if ((h = hexchar(str[i-1])) == -1) {
            WARNING(MAIN, "invalid hex char\n")
            return 1;
        }

        big->data[big->used++] = (h<<4) | l;
        if (big->used >= DATA_MAXLEN) { 
            ERROR(MAIN, "Bigint data overflow\n")
            return 1;
        }
    }

    if (i == 0) {
        if ((l = hexchar(str[i])) == -1) {
            WARNING(MAIN, "invalid hex char\n")
            return 1;
        }
        h = 0;

        big->data[big->used++] = (h<<4) | l;
        if (big->used >= DATA_MAXLEN) { 
            ERROR(MAIN, "Bigint data overflow\n")
            return 1;
        }
    }

    return 0;
}

ICACHE_FLASH_ATTR void bigint_expmod(Bigint *rv, Bigint *a,
                                     Bigint *b, Bigint *n) {
    /* FIXME STOPPED */
}

ICACHE_FLASH_ATTR static int8_t hexchar(char c) {
    if (48 <= c && c <= 57)
        return (c - 48) + 0;
    else if (65 <= c && c <= 70)
        return (c - 65) + 10;
    else if (97 <= c && c <= 102)
        return (c - 97) + 10;
    else
        return -1;
}
