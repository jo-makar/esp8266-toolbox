#ifndef BIGINT_H
#define BIGINT_H

#include <c_types.h>

typedef struct {
    uint16_t used; /* Bytes */
    #define DATA_MAXLEN 128
    uint8_t data[DATA_MAXLEN];
} Bigint;

void bigint_zero(Bigint *big);

/* Big endian hex to Bigint */
int bigint_fromhex(Bigint *big, const char *str);

/* rv = (a**b) % n */
void bigint_expmod(Bigint *rv, Bigint *a, Bigint *b, Bigint *n);

/*
 * TODO Add support for signed ints
 *      bigint_add => a + b
 *      bigint_sub => a - b
 *      bigint_mult => a * b
 *      bigint_rem => a % b
 *      bigint_addmod => (a + b) % n
 *      bigint_multmod => (a * b) % n
 */

#endif
