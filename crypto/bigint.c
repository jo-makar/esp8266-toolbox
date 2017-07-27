#include <osapi.h>

#include "bigint.h"
#include "missing-decls.h"

ICACHE_FLASH_ATTR void bigint_zero(Bigint *i) {
    i->sign = 0;
    i->used = 0;
    os_bzero(i->data, sizeof(i->data));
}

ICACHE_FLASH_ATTR int bigint_fromhex(Bigint *i, const char *s) {
    /* FIXME STOPPED */
    return 0;
}

/* rv = (a**b) % n */
ICACHE_FLASH_ATTR void bigint_expmod(Bigint *rv, Bigint *a,
                                     Bigint *b, Bigint *n) {
    /* FIXME STOPPED */
}
