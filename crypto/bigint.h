#ifndef BIGINT_H
#define BIGINT_H

#if STANDALONE
    #include <stdint.h>
#else
    #include <c_types.h>
#endif

typedef struct {
    uint16_t bytes;
    uint8_t bits;
    #define DATA_MAXLEN 150
    uint8_t data[DATA_MAXLEN];
} Bigint;

void bigint_zero(Bigint *i);

int bigint_fromhex(Bigint *i, const char *s);
void bigint_print(const Bigint *i, int size);

void bigint_copy(Bigint *d, const Bigint *s);

int bigint_iszero(const Bigint *i);

/* -1 if i<j, 0 if i=j, 1 if i>j */
int bigint_cmp(const Bigint *i, const Bigint *j);

int bigint_add(Bigint *s, const Bigint *a, const Bigint *b);

int bigint_mul(Bigint *p, const Bigint *a, const Bigint *b);

int bigint_div(Bigint *q, Bigint *r, const Bigint *a, const Bigint *b);

/* x = (a ** b) % c */
int bigint_expmod(Bigint *x, const Bigint *a, const Bigint *b, const Bigint *c);

#endif
