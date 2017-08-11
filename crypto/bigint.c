#if STANDALONE
    #include <sys/param.h>
    #include <stdio.h>
    #include <string.h>
    #include <strings.h>

    #define ICACHE_FLASH_ATTR /**/
#else
    #include "../log/log.h"
    #include "../missing-decls.h"

    #include <sys/param.h>
    #include <osapi.h>
#endif

#include "bigint.h"

Bigint mul_pt, mul_at;
Bigint div_qt, div_rt, div_bc;
Bigint expmod_xt, expmod_r;

#define BITS(i) (size_t)((i)->bytes * 8 + (i)->bits)

#undef BIT
#define BIT(i, n) \
    ((size_t)(n) < BITS(i) \
         ? (((i)->data[(n) / 8] >> ((n) % 8)) & 0x01) \
         : 0)

#define SET_BIT(i, n) { \
    if ((size_t)(n) < BITS(i)) \
        (i)->data[(n) / 8] |= 1 << ((n) % 8); \
}

#define CLR_BIT(i, n) { \
    if ((size_t)(n) < BITS(i)) \
        (i)->data[(n) / 8] &= ~(1 << ((n) % 8)); \
}

#define LEFT_SHIFT(i) { \
    uint16_t j; \
    uint8_t b, lb; \
    \
    lb = 0; \
    for (j = 0; j < (i)->bytes; j++) { \
        b = ((i)->data[j] >> 7) & 0x01; \
        (i)->data[j] = ((i)->data[j] << 1) | lb; \
        lb = b; \
    } \
    \
    (i)->data[j] = ((i)->data[j] << 1) | lb; \
    \
    if (++(i)->bits == 8) { \
        (i)->bytes++; \
        (i)->bits = 0; \
        if ((i)->bytes >= DATA_MAXLEN - 1) { \
            /* \
             * #if !STANDALONE \
             *     ERROR(CRYPTO, "data overflow") \
             * #endif \
             */ \
            return 1; \
        } \
    } \
}

#define TWOS_COMP(i) { \
    uint16_t j; \
    uint32_t b, k; \
    int32_t l; \
    \
    for (j = 0; j < (i)->bytes; j++) \
        (i)->data[j] = ~(i)->data[j]; \
    if ((i)->bits > 0) \
        (i)->data[j] = (~(i)->data[j]) & ((1 << (i)->bits) - 1); \
    \
    b = BITS(i); \
    for (k = 0; k < b; k++) \
        if (BIT(i, k) == 0) { \
            SET_BIT(i, k) \
            for (l = k - 1; l >= 0; l--) \
                CLR_BIT(i, l) \
            break; \
        } \
}

ICACHE_FLASH_ATTR void normalize(Bigint *i) {
    int32_t j;
    size_t u;

    /* Cleanup unused bits/bytes */

    if (i->bits > 0)
        i->data[i->bytes] &= (1 << i->bits) - 1;

    u = i->bytes + (i->bits > 0 ? 1 : 0);
    bzero(i->data + u, DATA_MAXLEN - u);

    /* Normalization proper */
    
    if (i->bits > 0) {
        if ((i->data[i->bytes] & ((1 << i->bits) - 1)) != 0)
            return;
        i->bits = 0;
    }
    
    for (j = i->bytes - 1; j >= 0; j--) {
        if (i->data[j] != 0)
            break;
    }
    i->bytes = j + 1;
    
    if (i->bytes == 0)
        i->bits = 1;
}

ICACHE_FLASH_ATTR uint32_t bigint_bits(const Bigint *i) {
    return BITS(i);
}

ICACHE_FLASH_ATTR void bigint_zero(Bigint *i) {
    i->bytes = 0;
    i->bits = 1;
    bzero(i->data, sizeof(i->data));
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

ICACHE_FLASH_ATTR int bigint_fromhex(Bigint *i, const char *s) {
    int16_t j;
    int8_t h, l;

    bigint_zero(i);
    i->bits = 0;


    for (j = strlen(s) - 1; j >= 1; j -= 2) {
        if ((l = hexchar(s[j])) == -1) {
            #if !STANDALONE
                WARNING(CRYPTO, "invalid hex char")
            #endif
            return 2;
        }
        if ((h = hexchar(s[j-1])) == -1) {
            #if !STANDALONE
                WARNING(CRYPTO, "invalid hex char")
            #endif
            return 2;
        }

        i->data[i->bytes++] = (h << 4) | l;
        if (i->bytes >= DATA_MAXLEN - 1) {
            #if !STANDALONE
                ERROR(CRYPTO, "data overflow")
            #endif
            return 1;
        }
    }

    if (j == 0) {
        if ((l = hexchar(s[j])) == -1) {
            #if !STANDALONE
                WARNING(CRYPTO, "invalid hex char")
            #endif
            return 2;
        }

        i->data[i->bytes] = l;
        i->bits = 4;
    }

    return 0;
}

ICACHE_FLASH_ATTR void bigint_print(const Bigint *i, int size) {
    int32_t j;

    #if STANDALONE
        #define PRINTF printf
    #else
        #define PRINTF os_printf
    #endif

    if (size)
        PRINTF("%u.%u ", i->bytes, i->bits);

    if (i->bits > 0)
        PRINTF("%02x", i->data[i->bytes] & ((1 << i->bits) - 1));

    for (j = i->bytes - 1; j >= 0; j--)
        PRINTF("%02x", i->data[j]);
}

ICACHE_FLASH_ATTR void bigint_copy(Bigint *d, const Bigint *s) {
    d->bytes = s->bytes;
    d->bits = s->bits;
    bzero(d->data, sizeof(d->data));
    memcpy(d->data, s->data, d->bytes + (d->bits > 0 ? 1 : 0));

    if (d->bits > 0) 
        d->data[d->bytes] &= (1 << d->bits) - 1;
}

ICACHE_FLASH_ATTR int bigint_iszero(const Bigint *i) {
    uint16_t j;

    for (j = 0; j < i->bytes; j++)
        if (i->data[j] > 0)
            return 0;

    if (i->bits > 0)
        if ((i->data[i->bytes] & ((1 << i->bits) - 1)) > 0)
            return 0;

    return 1;
}

ICACHE_FLASH_ATTR int bigint_cmp(const Bigint *i, const Bigint *j) {
    int32_t k;
    uint8_t l, r;

    if (BITS(i) > BITS(j)) {
        for (k = BITS(i) - 1; k >= (int32_t)BITS(j); k--)
            if (BIT(i, k) == 1)
                return 1;
    }
    else if (BITS(i) < BITS(j)) {
        for (k = BITS(j) - 1; k >= (int32_t)BITS(i); k--)
            if (BIT(j, k) == 1)
                return -1;
    }
    else
        k = BITS(i) - 1;

    /* Compare the leading bits */
    for (; k % 8 != 0 && k >= 0; k--) {
        l = BIT(i, k);
        r = BIT(j, k);

        if (l > r)
            return 1;
        else if (l < r)
            return -1;
    }

    /* Compare the remaining bytes */
    for (; k / 8 >= 0; k -= 8) {
        if (i->data[k / 8] > j->data[k / 8])
            return 1;
        else if (i->data[k / 8] < j->data[k / 8])
            return -1;
    }

    return 0;
}

ICACHE_FLASH_ATTR int bigint_add(Bigint *s, const Bigint *a, const Bigint *b) {
    uint32_t i, m;
    uint8_t n, c;

    m = MAX(BITS(a), BITS(b));
    c = 0;

    for (i = 0; i < m; i++) {
        if (i >= BITS(s)) {
            s->bytes = (i + 1) / 8;
            s->bits = (i + 1) % 8;
            if (s->bytes >= DATA_MAXLEN - 1) {
                #if !STANDALONE
                    ERROR(CRYPTO, "data overflow")
                #endif
                return 1;
            }
        }

        n = BIT(a, i) + BIT(b, i) + c;

        if      (n == 3)   { SET_BIT(s, i) c = 1; }
        else if (n == 2)   { CLR_BIT(s, i) c = 1; }
        else if (n == 1)   { SET_BIT(s, i) c = 0; }
        else  /* n == 0 */ { CLR_BIT(s, i) c = 0; }
    }

    if (c == 1) {
        if (i >= BITS(s)) {
            s->bytes = (i + 1) / 8;
            s->bits = (i + 1) % 8;
            if (s->bytes >= DATA_MAXLEN - 1) {
                #if !STANDALONE
                    ERROR(CRYPTO, "data overflow")
                #endif
                return 1;
            }
        }

        SET_BIT(s, i)
        
        if (i >= BITS(s)) {
            s->bytes = i / 8;
            s->bits = i % 8;
            if (s->bytes >= DATA_MAXLEN - 1) {
                #if !STANDALONE
                    ERROR(CRYPTO, "data overflow")
                #endif
                return 1;
            }
        }
    }

    if (BITS(s) > m + c) {
        s->bytes = (m + c) / 8;
        s->bits = (m + c) % 8;
        normalize(s);
    }

    return 0;
}

ICACHE_FLASH_ATTR int bigint_mul(Bigint *p, const Bigint *a, const Bigint *b) {
    uint32_t i;
    int rv;

    bigint_zero(&mul_pt);
    bigint_copy(&mul_at, a);

    for (i = 0; i < BITS(b); i++) {
        #if !STANDALONE
            if (i > 0 && i % 10 == 0)
                system_soft_wdt_feed();
        #endif

        if (BIT(b, i) == 1) {
            if ((rv = bigint_add(&mul_pt, &mul_pt, &mul_at)) > 0)
                return rv;
        }

        LEFT_SHIFT(&mul_at)
    }

    bigint_copy(p, &mul_pt);

    return 0;
}

ICACHE_FLASH_ATTR int bigint_div(Bigint *q, Bigint *r,
                                 const Bigint *a, const Bigint *b) {
    /*
     * q := a / b, r := a % b
     * where bn is the number of bits in b
     *
     * if b = 0 then error end
     * q := 0
     * r := 0
     * for i := bn − 1 .. 0 do
     *   r := l << 1
     *   r(0) := a(i)
     *   if r >= b then
     *     r := r − b
     *     q(i) := 1
     *   end
     * end
     */

    int32_t i;
    int rv;

    if (bigint_iszero(b)) {
        #if !STANDALONE
            WARNING(CRYPTO, "division by zero")
        #endif
        return 2;
    }

    if (q == r) {
        #if !STANDALONE
            ERROR(CRYPTO, "assert q != r")
        #endif
        return 2;
    }

    bigint_zero(&div_qt);
    bigint_zero(&div_rt);

    bigint_copy(&div_bc, b);
    TWOS_COMP(&div_bc)

    for (i = BITS(a) - 1; i >= 0; i--) {
        #if !STANDALONE
            if (i % 10 == 0)
                system_soft_wdt_feed();
        #endif

        LEFT_SHIFT(&div_rt)
        div_rt.data[0] |= BIT(a, i);

        if (++div_qt.bits == 8) {
            div_qt.bytes++;
            div_qt.bits = 0;
            if (div_qt.bytes >= DATA_MAXLEN - 1) {
                #if !STANDALONE
                    ERROR(CRYPTO, "data overflow")
                #endif
                return 1;
            }
        }

        if (bigint_cmp(&div_rt, b) >= 0) {
            /* r = r + (-b) */

            if ((rv = bigint_add(&div_rt, &div_rt, &div_bc)) > 0)
                return rv;

            if (BITS(&div_rt) > BITS(b)) {
                div_rt.bytes = b->bytes;
                div_rt.bits = b->bits;
                normalize(&div_rt);
            }

            if (bigint_cmp(&div_rt, b) >= 0) {
                #if !STANDALONE
                    ERROR(CRYPTO, "assert r < b")
                #endif
                return 3;
            }

            SET_BIT(&div_qt, i)
        }
    }

    if (q != NULL) {
        normalize(&div_qt);
        bigint_copy(q, &div_qt);
    }
    if (r != NULL) {
        normalize(&div_rt);
        bigint_copy(r, &div_rt);
    }

    return 0;
}

ICACHE_FLASH_ATTR int bigint_expmod(Bigint *x, const Bigint *a,
                                    const Bigint *b, const Bigint *c) {
    /*
     * x := (a ** b) % c
     * where bn is the number of bits in b
     *
     * x := 1
     * r := a % c
     * for i := bn - 1 .. 0 do
     *   x := (x ** 2) % c
     *   if b(i) = 1 then
     *     x := (x * r) % c
     *   end
     * end
     */

    int rv;
    int32_t i;

    bigint_zero(&expmod_xt);
    SET_BIT(&expmod_xt, 0)

    if ((rv = bigint_div(NULL, &expmod_r, a, c)) > 0)
        return rv;

    for (i = BITS(b) - 1; i >= 0; i--) {
        #if !STANDALONE
            if (i % 10 == 0)
                DEBUG(CRYPTO, "expmod i=%u", i)
        #endif

        /* x = (x ** 2) % c */
        if ((rv = bigint_mul(&expmod_xt, &expmod_xt, &expmod_xt)) > 0)
            return rv;
        if ((rv = bigint_div(NULL, &expmod_xt, &expmod_xt, c)) > 0)
            return rv;

        if (BIT(b, i) == 1) {
            /* x = (x * r) % c */
            if ((rv = bigint_mul(&expmod_xt, &expmod_xt, &expmod_r)) > 0)
                return rv;
            if ((rv = bigint_div(NULL, &expmod_xt, &expmod_xt, c)) > 0)
                return rv;
        }
    }

    bigint_copy(x, &expmod_xt);

    return 0;
}
