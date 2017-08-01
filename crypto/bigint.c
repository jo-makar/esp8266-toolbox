#include <sys/param.h>
#include <osapi.h>

#include "../log.h"
#include "bigint.h"
#include "missing-decls.h"

/*
 * To avoid using stack space define some temp variables here.
 * The t variables are for top-level functions, u's for nested functions.
 * Ie u1 and u2 are used in bigint_divmod() which is called by bigint_expmod().
 */
static Bigint t1, t2, t3, t4, u1, u2;

static int8_t hexchar(char c);

#define BIGINT_LEFTSHIFT(x) { \
    uint16_t i; \
    uint8_t bit, lastbit=0; \
    \
    for (i=0; i<(x)->bytes; i++) { \
        bit = (x)->data[i] >> 7; \
        (x)->data[i] = ((x)->data[i]<<1) | lastbit; \
        lastbit = bit; \
    } \
    \
    (x)->data[i] = ((x)->data[i]<<1) | lastbit; \
    \
    if (++(x)->bits == 8) { \
        (x)->bytes++; \
        (x)->bits = 0; \
        if ((x)->bytes >= DATA_MAXLEN-1) { \
            ERROR(MAIN, "data overflow\n") \
            return 1; \
        } \
    } \
}

ICACHE_FLASH_ATTR void bigint_norm(Bigint *i) {
    int32_t idx;

    if (i->bits > 0) {
        if ((i->data[i->bytes] & ((1<<i->bits)-1)) != 0)
            return;
        i->bits = 0;
    }

    for (idx=i->bytes-1; idx>=0; idx--) {
        if (i->data[idx] != 0)
            break;
    }
    i->bytes = idx + 1;

    if (i->bytes == 0)
        i->bits = 1;

    return;
}

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

ICACHE_FLASH_ATTR int bigint_addto(Bigint *s, const Bigint *a) {
    uint32_t i;
    uint32_t bits;
    uint8_t carry = 0;
    uint8_t n;

    if (s == a) {
        ERROR(MAIN, "assert s != a\n")
        return 1;
    }

    bits = BIGINT_BITS(a);
    for (i=0; i<bits; i++) {
        n = BIGINT_BIT(s, i) + BIGINT_BIT(a, i) + carry;
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

        if (i == (uint32_t)BIGINT_BITS(s)) {
            if (++s->bits == 8) {
                s->bytes++;
                s->bits = 0;
                if (s->bytes >= DATA_MAXLEN-1) {
                    ERROR(MAIN, "data overflow\n")
                    return 1;
                }
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
    uint32_t idx;
    uint32_t i;
    int rv;

    if (p == a || p == b) {
        ERROR(MAIN, "assert p!=a && p!=b\n")
        return 1;
    }

    bigint_zero(p);

    for (idx=0; idx<(uint32_t)BIGINT_BITS(b); idx++) {
        if (idx > 0 && idx % 10 == 0)
            system_soft_wdt_feed();

        if (BIGINT_BIT(b, idx)) {
            bigint_copy(&u1, a);
            for (i=0; i<idx; i++)
                BIGINT_LEFTSHIFT(&u1)

            if ((rv = bigint_addto(p, &u1)) > 0)
                return rv;
        }
    }

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
     * for i := n − 1 .. 0 do    -- Where n is the number of bits in N
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
    int rv;

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

        bigint_copy(&u1, d);

        /* Flip the bits */
        for (i=0; i<u1.bytes; i++)
            u1.data[i] = ~u1.data[i];
        if (u1.bits > 0) {
            u1.data[i] = ~u1.data[i];
            u1.data[i] &= (1<<u1.bits) - 1;
        }

        /* Add one */
        bits = BIGINT_BITS(&u1);
        for (j=0; j<bits; j++)
            if (BIGINT_BIT(&u1, j) == 0) {
                BIGINT_SETBIT(&u1, j)
                for (k=j-1; k>=0; k--)
                    BIGINT_CLRBIT(&u1, k)
                break;
            }
        /* assert j != BIGINT_BITS(&u1), because d != 0 */
    }

    for (i=BIGINT_BITS(n)-1; i>=0; i--) {
        if (i % 10 == 0)
            system_soft_wdt_feed();

        BIGINT_LEFTSHIFT(r)
        if (BIGINT_BITS(r) > BIGINT_BITS(d)) {
            r->bytes = d->bytes;
            r->bits = d->bits;
        }

        r->data[0] |= BIGINT_BIT(n, i);

        if (bigint_cmp(r, d) >= 0) {
            /*
             * r = r + (-d)
             * Also maintain length of bits in r
             */
            if ((rv = bigint_add(&u2, r, &u1)) > 0)
                return rv;
            u2.bytes = r->bytes;
            u2.bits = r->bits;
            bigint_copy(r, &u2);

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
    /*
     * X := (A^B) % N
     *
     * X := 1
     * for i := b - 2 .. 0 do       -- Where b is the number of bits in B
     *   X := (X^2) % N
     *   if B(i) = 1 then
     *     X := (X*A) % N
     *   end
     * end
     */

    int32_t i;
    int rv;
 
    if (x == a || x == b || x == n) {
        ERROR(MAIN, "assert x!=a && x!=b && x!=n\n")
        return 1;
    }

    bigint_zero(x);
    BIGINT_SETBIT(x, 0)

    /*
     * Calculate x = (t4^b) % n where t4 = a%n
     * to reduce storage requirements
     */
    if ((rv = bigint_divmod(&t3, &t4, a, n)) > 0)
        return rv;
    bigint_norm(&t4);

    for (i=BIGINT_BITS(b)-2; i>=0; i--) {
        DEBUG(MAIN, "bigint_expmod: i=%u\n", i)

        /* x = x**2 */
        bigint_copy(&t1, x);
        if ((rv = bigint_mult(&t2, x, &t1)) > 0)
            return rv;
        bigint_copy(x, &t2);

        /* x %= n */
        if ((rv = bigint_divmod(&t1, &t2, x, n)) > 0)
            return rv;
        bigint_norm(&t2);
        bigint_copy(x, &t2);

        if (BIGINT_BIT(b, i) == 1) {
            /* x *= t4 */
            if ((rv = bigint_mult(&t1, x, &t4)) > 0)
                return rv;
            bigint_copy(x, &t1);

            /* x %= n */
            if ((rv = bigint_divmod(&t1, &t2, x, n)) > 0)
                return rv;
            bigint_norm(&t2);
            bigint_copy(x, &t2);
        }
    }

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
