#include "../missing-decls.h"
#include "md5.h"

#include <sys/param.h>
#include <osapi.h>

ICACHE_RODATA_ATTR const uint32_t md5_k[64] = {
    0xd76aa478, 0xe8c7b756, 0x242070db, 0xc1bdceee, 0xf57c0faf,
    0x4787c62a, 0xa8304613, 0xfd469501, 0x698098d8, 0x8b44f7af,
    0xffff5bb1, 0x895cd7be, 0x6b901122, 0xfd987193, 0xa679438e,
    0x49b40821, 0xf61e2562, 0xc040b340, 0x265e5a51, 0xe9b6c7aa,
    0xd62f105d, 0x02441453, 0xd8a1e681, 0xe7d3fbc8, 0x21e1cde6,
    0xc33707d6, 0xf4d50d87, 0x455a14ed, 0xa9e3e905, 0xfcefa3f8,
    0x676f02d9, 0x8d2a4c8a, 0xfffa3942, 0x8771f681, 0x6d9d6122,
    0xfde5380c, 0xa4beea44, 0x4bdecfa9, 0xf6bb4b60, 0xbebfbc70,
    0x289b7ec6, 0xeaa127fa, 0xd4ef3085, 0x04881d05, 0xd9d4d039,
    0xe6db99e5, 0x1fa27cf8, 0xc4ac5665, 0xf4292244, 0x432aff97,
    0xab9423a7, 0xfc93a039, 0x655b59c3, 0x8f0ccc92, 0xffeff47d,
    0x85845dd1, 0x6fa87e4f, 0xfe2ce6e0, 0xa3014314, 0x4e0811a1,
    0xf7537e82, 0xbd3af235, 0x2ad7d2bb, 0xeb86d391,
};

const uint8_t md5_s[64] = {
    7, 12, 17, 22,  7, 12, 17, 22,  7, 12, 17, 22,  7, 12, 17, 22,
    5,  9, 14, 20,  5,  9, 14, 20,  5,  9, 14, 20,  5,  9, 14, 20,
    4, 11, 16, 23,  4, 11, 16, 23,  4, 11, 16, 23,  4, 11, 16, 23,
    6, 10, 15, 21,  6, 10, 15, 21,  6, 10, 15, 21,  6, 10, 15, 21,
};

#define LROTATE(x, n) ((x << n) | (x >> (32-n)))

static void md5_consume(Md5State *state, const uint8_t *chunk);

ICACHE_FLASH_ATTR void md5_init(Md5State *state) {
    state->h[0] = 0x67452301;
    state->h[1] = 0xefcdab89;
    state->h[2] = 0x98badcfe;
    state->h[3] = 0x10325476;

    state->buflen = 0;
    state->totallen = 0;
}

ICACHE_FLASH_ATTR void md5_proc(Md5State *state,
                                const uint8_t *buf, size_t len) {
    size_t used = 0;
    size_t len2;

    while (len-used > 0) {
        len2 = MIN(64-state->buflen, len-used);
        os_memcpy(state->buf+state->buflen, buf+used, len2);
        state->buflen += len2;
        used += len2;

        if (state->buflen == 64) {
            md5_consume(state, state->buf);
            state->buflen = 0;
            state->totallen += 64;
        }
    }
}

ICACHE_FLASH_ATTR void md5_done(Md5State *state, uint8_t *hash) {
    uint64_t total;
    uint8_t i;

    /* assert state->buflen < 64 */

    if (state->buflen < 56) {
        state->totallen += state->buflen;

        state->buf[state->buflen++] = 0x80;
        os_bzero(state->buf+state->buflen, 56-state->buflen);
    }

    else {
        state->totallen += state->buflen;

        state->buf[state->buflen++] = 0x80;
        os_bzero(state->buf+state->buflen, 64-state->buflen);
        md5_consume(state, state->buf);

        os_bzero(state->buf, 56);
    }

    total = state->totallen * 8;

    state->buf[63] = total >> (24+32);
    state->buf[62] = total >> (16+32);
    state->buf[61] = total >> (8+32);
    state->buf[60] = total >> 32;

    state->buf[59] = total >> 24;
    state->buf[58] = total >> 16;
    state->buf[57] = total >> 8;
    state->buf[56] = total;

    md5_consume(state, state->buf);

    for (i=0; i<4; i++) {
        *(hash+i*4  ) = state->h[i];
        *(hash+i*4+1) = state->h[i] >> 8;
        *(hash+i*4+2) = state->h[i] >> 16;
        *(hash+i*4+3) = state->h[i] >> 24;
    }
}

ICACHE_FLASH_ATTR void md5(const uint8_t *buf, size_t len, uint8_t *hash) {
    Md5State s;

    md5_init(&s);
    md5_proc(&s, buf, len);
    md5_done(&s, hash);
}

ICACHE_FLASH_ATTR void md5_consume(Md5State *state, const uint8_t *chunk) {
    int i;
    uint32_t m[16];
    uint32_t hh[4];
    uint32_t f, g;

    os_bzero(m, sizeof(m));

    for (i=0; i<16; i++)
        m[i] = (chunk[i*4+3]<<24) | (chunk[i*4+2]<<16) |
               (chunk[i*4+1]<<8)  |  chunk[i*4  ];

    for (i=0; i<4; i++)
        hh[i] = state->h[i];

    for (i=0; i<64; i++) {
        if (0 <= i && i <= 15) {
            f = (hh[1] & hh[2]) | (~hh[1] & hh[3]);
            g = i;
        } else if (16 <= i && i <= 31) {
            f = (hh[3] & hh[1]) | (~hh[3] & hh[2]);
            g = (5*i + 1) % 16;
        } else if (32 <= i && i <= 47) {
            f = hh[1] ^ hh[2] ^ hh[3];
            g = (3*i + 5) % 16;
        } else if (48 <= i && i <= 63) {
            f = hh[2] ^ (hh[1] | ~hh[3]);
            g = (7*i) % 16;
        }

        f = f + hh[0] + md5_k[i] + m[g];
        hh[0] = hh[3];
        hh[3] = hh[2];
        hh[2] = hh[1];
        hh[1] = hh[1] + LROTATE(f, md5_s[i]);
    }

    for (i=0; i<4; i++)
        state->h[i] += hh[i];
}
