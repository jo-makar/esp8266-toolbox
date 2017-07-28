#include <sys/param.h>
#include <osapi.h>

#include "../log.h"
#include "bigint.h"
#include "missing-decls.h"

static int8_t hexchar(char c);

ICACHE_FLASH_ATTR void bigint_zero(Bigint *i) {
    i->bytes = 0;
    i->bits = 1;
    os_bzero(i->data, sizeof(i->data));
}

ICACHE_FLASH_ATTR void bigint_copy(Bigint *d, const Bigint *s) {
    d->bytes = s->bytes;
    d->bits = s->bits;
    os_bzero(d->data, sizeof(d->data));
    os_memcpy(d->data, s->data, d->bytes + (d->bits>0 ? 1 : 0));
}

ICACHE_FLASH_ATTR int bigint_fromhex(Bigint *i, const char *s) {
    size_t len = os_strlen(s);
    int j;
    int8_t h, l;

    i->bytes = 0;
    i->bits = 0;
    os_bzero(i->data, sizeof(i->data));

    for (j=len-1; j>=1; j-=2) {
        if ((l = hexchar(s[j])) == -1) {
            WARNING(MAIN, "invalid hex char\n")
            return 1;
        }
        if ((h = hexchar(s[j-1])) == -1) {
            WARNING(MAIN, "invalid hex char\n")
            return 1;
        }

        i->data[i->bytes++] = (h<<4) | l;
        if (i->bytes >= DATA_MAXLEN-1) { 
            ERROR(MAIN, "data overflow\n")
            return 1;
        }
    }

    if (j == 0) {
        if ((l = hexchar(s[j])) == -1) {
            WARNING(MAIN, "invalid hex char\n")
            return 1;
        }
        h = 0;

        i->data[i->bytes++] = (h<<4) | l;
        if (i->bytes >= DATA_MAXLEN-1) { 
            ERROR(MAIN, "data overflow\n")
            return 1;
        }
    }

    return 0;
}

ICACHE_FLASH_ATTR void bigint_print(const Bigint *i) {
    int32_t j;

    os_printf("%u %u ", i->bits, i->bytes);

    if (i->bits > 0)
        os_printf("%02x", i->data[i->bytes] & ((1<<i->bits)-1));

    for (j=i->bytes-1; j>=0; j--)
        os_printf("%02x", i->data[j]);
}

ICACHE_FLASH_ATTR int bigint_iszero(const Bigint *i) {
    uint16_t j;
    
    if (BIGINT_BITS(i) == 0)
        return 0;

    for (j=0; j<i->bytes; j++)
        if (i->data[j] > 0)
            return 0;

    if (i->bits > 0)
        if (i->data[i->bytes] & ((1<<(i->bits+1))-1))
            return 0;

    return 1;
}

ICACHE_FLASH_ATTR int bigint_cmp(const Bigint *i, const Bigint *j) {
    int32_t idx;
    uint8_t l, r;

    if (BIGINT_BITS(i) > BIGINT_BITS(j)) {
        for (idx=BIGINT_BITS(i)-1; idx>=BIGINT_BITS(j); idx--)
            if (BIGINT_BIT(i, idx) == 1)
                return 1;
    } else if (BIGINT_BITS(i) < BIGINT_BITS(j)) {
        for (idx=BIGINT_BITS(j)-1; idx>=BIGINT_BITS(i); idx--)
            if (BIGINT_BIT(j, idx) == 1)
                return -1;
    }
    else
        idx = BIGINT_BITS(i) - 1;

    /* Compare the leading bits */
    for (; idx%8!=0 && idx>=0; idx--) {
        l = BIGINT_BIT(i, idx);
        r = BIGINT_BIT(j, idx);

        if (l > r)
            return 1;
        else if (l < r)
            return -1;
    }

    /* Compare the remaining bytes */
    for (; idx/8>=0; idx-=8) {
        if (i->data[idx] > j->data[idx])
            return 1;
        else if (i->data[idx] < j->data[idx])
            return -1;
    }

    return 0;
}

ICACHE_FLASH_ATTR int bigint_add(Bigint *s, const Bigint *a, const Bigint *b) {
    uint32_t i;
    uint32_t bits = MAX(BIGINT_BITS(a), BIGINT_BITS(b));
    uint8_t carry = 0;
    uint8_t n;

    if (s == a || s == b) {
        ERROR(MAIN, "assert s!=a && s!=b\n")
        return 1;
    }

    bigint_zero(s);

    for (i=0; i<bits; i++) {
        n = BIGINT_BIT(a, i) + BIGINT_BIT(b, i) + carry;
        if (n == 3) {
            BIGINT_SETBIT(s, i)
            carry = 1;
        } else if (n == 2) {
            BIGINT_CLRBIT(s, i)
            carry = 1;
        } else if (n == 1) {
            BIGINT_SETBIT(s, i)
            carry = 0;
        } else /* n == 0 */ {
            BIGINT_CLRBIT(s, i)
            carry = 0;
        }

        if (++s->bits == 8) {
            s->bytes++;
            s->bits = 0;
            if (s->bytes >= DATA_MAXLEN-1) {
                ERROR(MAIN, "data overflow\n")
                return 1;
            }
        }
    }

    if (carry) {
        BIGINT_SETBIT(s, i)

        if (++s->bits == 8) {
            s->bytes++;
            s->bits = 0;
            if (s->bytes >= DATA_MAXLEN-1) {
                ERROR(MAIN, "data overflow\n")
                return 1;
            }
        }
    }

    return 0;
}

ICACHE_FLASH_ATTR int bigint_mult(Bigint *p, const Bigint *a, const Bigint *b) {
    if (p == a || p == b) {
        ERROR(MAIN, "assert p!=a && p!=b\n")
        return 1;
    }

    /* FIXME STOPPED */

    return 0;
}

ICACHE_FLASH_ATTR int bigint_divmod(Bigint *q, Bigint *r,
                                    const Bigint *n, const Bigint *d) {
    /*
     * Q := N / D, R := N % D
     *
     * if D = 0 then error end   -- Division by zero
     * Q := 0                    -- Initialize quotient and remainder to zero
     * R := 0                     
     * for i := n − 1 .. 0 do    -- Where n is number of bits in N
     *   R := R << 1             -- Left-shift R by 1 bit
     *   R(0) := N(i)            -- Set the least-significant bit of R equal to
     *                           -- bit i of the numerator
     *   if R >= D then
     *     R := R − D
     *     Q(i) := 1
     *   end
     * end
     */

    int32_t i;
    Bigint dc;
    Bigint s;

    if (bigint_iszero(d)) {
        WARNING(MAIN, "division by zero\n")
        return 1;
    }

    if (q == r || q == n || q == d) {
        ERROR(MAIN, "assert q!=r && q!=n && q!=d\n")
        return 1;
    }
    if (r == n || r == d) {
        ERROR(MAIN, "assert r!=n && r!=d\n")
        return 1;
    }

    bigint_zero(q);
    bigint_zero(r);

    /*
     * Two's complement
     */
    {
        uint16_t i;
        uint32_t j;
        int32_t k;
        uint32_t bits;

        bigint_copy(&dc, d);

        /* Flip the bits */
        for (i=0; i<dc.bytes; i++)
            dc.data[i] = ~dc.data[i];
        if (dc.bits > 0) {
            dc.data[i] = ~dc.data[i];
            dc.data[i] &= (1<<dc.bits) - 1;
        }

        /* Add one */
        bits = BIGINT_BITS(&dc);
        for (j=0; j<bits; j++)
            if (BIGINT_BIT(&dc, j) == 0) {
                BIGINT_SETBIT(&dc, j)
                for (k=j-1; k>=0; k--)
                    BIGINT_CLRBIT(&dc, k)
                break;
            }
        /* assert j != BIGINT_BITS(&dc), because d != 0 */
    }


    #define BIGINT_LEFTSHIFT(x) { \
        uint16_t i; \
        uint8_t bit, lastbit=0; \
        \
        for (i=0; i<x->bytes; i++) { \
            bit = x->data[i] >> 7; \
            x->data[i] = (x->data[i]<<1) | lastbit; \
            lastbit = bit; \
        } \
        \
        x->data[i] = (x->data[i]<<1) | lastbit; \
        \
        if (++x->bits == 8) { \
            x->bytes++; \
            x->bits = 0; \
            if (x->bytes >= DATA_MAXLEN-1) { \
                ERROR(MAIN, "data overflow\n") \
                return 1; \
            } \
        } \
    }

    for (i=BIGINT_BITS(n)-1; i>=0; i--) {
        BIGINT_LEFTSHIFT(r)

        r->data[0] |= BIGINT_BIT(n, i);

        if (bigint_cmp(r, d) >= 0) {
            /* r = r + (-d) */
            bigint_add(&s, r, &dc);
            s.bytes = r->bytes;
            s.bits = r->bits;
            bigint_copy(r, &s);

            BIGINT_SETBIT(q, i)
        }

        if (++q->bits == 8) {
            q->bytes++;
            q->bits = 0;
            if (q->bytes >= DATA_MAXLEN-1) {
                ERROR(MAIN, "data overflow\n")
                return 1;
            }
        }
    }

    return 0;
}

ICACHE_FLASH_ATTR int bigint_expmod(Bigint *x, const Bigint *a,
                                    const Bigint *b, const Bigint *n) {
    if (x == a || x == b || x == n) {
        ERROR(MAIN, "assert x!=a && x!=b && x!=n\n")
        return 1;
    }

    /* FIXME STOPPED */

    return 0;
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
