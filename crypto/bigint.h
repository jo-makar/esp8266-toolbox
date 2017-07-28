#ifndef BIGINT_H
#define BIGINT_H

#include <c_types.h>

typedef struct {
    uint16_t bytes;
    uint8_t bits;
    #define DATA_MAXLEN 128
    uint8_t data[DATA_MAXLEN];
} Bigint;

#define BIGINT_BITS(i) ((i)->bytes*8 + (i)->bits)

/* TODO Should verify n <= BIGINT_BITS(i) */
#define BIGINT_BIT(i, n) (((i)->data[(n)/8] >> ((n)%8)) & 0x01)
#define BIGINT_SETBIT(i, n) (i)->data[(n)/8] |= 1 << ((n)%8);
#define BIGINT_CLRBIT(i, n) (i)->data[(n)/8] &= ~(1 << ((n)%8));

void bigint_zero(Bigint *i);

void bigint_copy(Bigint *d, const Bigint *s);

/* Big endian hex to Bigint */
int bigint_fromhex(Bigint *i, const char *s);

void bigint_print(const Bigint *i);

int bigint_iszero(const Bigint *i);

/* -1 if i<j, 0 if i=j, 1 if i>j */
int bigint_cmp(const Bigint *i, const Bigint *j);

/* s = a + b */
int bigint_add(Bigint *s, const Bigint *a, const Bigint *b);

/* p = a * b */
int bigint_mult(Bigint *p, const Bigint *a, const Bigint *b);

/* q = n / d, r = n % d */
int bigint_divmod(Bigint *q, Bigint *r, const Bigint *n, const Bigint *d);

/* x = (a**b) % n */
int bigint_expmod(Bigint *x, const Bigint *a, const Bigint *b, const Bigint *n);

/*
 * TODO Add support for signed ints
 *      bigint_sub => a - b
 *      bigint_mult => a * b
 *      bigint_addmod => (a + b) % n
 *      bigint_multmod => (a * b) % n
 */

#endif
