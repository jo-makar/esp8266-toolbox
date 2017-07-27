#ifndef BIGINT_H
#define BIGINT_H

#include <c_types.h>

typedef struct {
    uint8_t sign;
    uint16_t used; /* Bytes */
    #define DATA_MAXLEN 128
    uint8_t data[DATA_MAXLEN];
} Bigint;

void bigint_zero(Bigint *i);
int bigint_fromhex(Bigint *i, const char *s);

/* rv = (a**b) % n */
void bigint_expmod(Bigint *rv, Bigint *a, Bigint *b, Bigint *n);

/*
 * TODO bigint_add => a + b
 *      bigint_sub => a - b
 *      bigint_mult => a * b
 *      bigint_rem => a % b
 *      bigint_addmod => (a + b) % n
 *      bigint_multmod => (a * b) % n
 */

#endif
