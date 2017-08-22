#include "../missing-decls.h"
#include "sha256.h"

#include <sys/param.h>
#include <osapi.h>

ICACHE_RODATA_ATTR const uint32_t sha256_k[64] = {
    0x428a2f98, 0x71374491, 0xb5c0fbcf, 0xe9b5dba5, 0x3956c25b,
    0x59f111f1, 0x923f82a4, 0xab1c5ed5, 0xd807aa98, 0x12835b01,
    0x243185be, 0x550c7dc3, 0x72be5d74, 0x80deb1fe, 0x9bdc06a7,
    0xc19bf174, 0xe49b69c1, 0xefbe4786, 0x0fc19dc6, 0x240ca1cc,
    0x2de92c6f, 0x4a7484aa, 0x5cb0a9dc, 0x76f988da, 0x983e5152,
    0xa831c66d, 0xb00327c8, 0xbf597fc7, 0xc6e00bf3, 0xd5a79147,
    0x06ca6351, 0x14292967, 0x27b70a85, 0x2e1b2138, 0x4d2c6dfc,
    0x53380d13, 0x650a7354, 0x766a0abb, 0x81c2c92e, 0x92722c85,
    0xa2bfe8a1, 0xa81a664b, 0xc24b8b70, 0xc76c51a3, 0xd192e819,
    0xd6990624, 0xf40e3585, 0x106aa070, 0x19a4c116, 0x1e376c08,
    0x2748774c, 0x34b0bcb5, 0x391c0cb3, 0x4ed8aa4a, 0x5b9cca4f,
    0x682e6ff3, 0x748f82ee, 0x78a5636f, 0x84c87814, 0x8cc70208,
    0x90befffa, 0xa4506ceb, 0xbef9a3f7, 0xc67178f2
};

static uint32_t rrotate(uint32_t x, uint8_t n);

static void sha256_consume(Sha256State *state, const uint8_t *chunk);

ICACHE_FLASH_ATTR void sha256_init(Sha256State *state) {
    state->h[0] = 0x6a09e667;
    state->h[1] = 0xbb67ae85;
    state->h[2] = 0x3c6ef372;
    state->h[3] = 0xa54ff53a;
    state->h[4] = 0x510e527f;
    state->h[5] = 0x9b05688c;
    state->h[6] = 0x1f83d9ab;
    state->h[7] = 0x5be0cd19;

    state->buflen = 0;
    state->totallen = 0;
}

ICACHE_FLASH_ATTR void sha256_proc(Sha256State *state, const uint8_t *buf, size_t len) {
    size_t used = 0;
    size_t len2;

    while (len-used > 0) {
        len2 = MIN(64-state->buflen, len-used);
        os_memcpy(state->buf+state->buflen, buf+used, len2);
        state->buflen += len2;
        used += len2;

        if (state->buflen == 64) {
            sha256_consume(state, state->buf);
            state->buflen = 0;
            state->totallen += 64;
        }
    }
}

ICACHE_FLASH_ATTR void sha256_done(Sha256State *state, uint8_t *hash) {
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
        sha256_consume(state, state->buf);

        os_bzero(state->buf, 56);
    }

    total = state->totallen * 8;

    state->buf[56] = total >> (24+32);
    state->buf[57] = total >> (16+32);
    state->buf[58] = total >> (8+32);
    state->buf[59] = total >> 32;

    state->buf[60] = total >> 24;
    state->buf[61] = total >> 16;
    state->buf[62] = total >> 8;
    state->buf[63] = total;

    sha256_consume(state, state->buf);

    for (i=0; i<8; i++) {
        *(hash+i*4+0) = state->h[i] >> 24;
        *(hash+i*4+1) = state->h[i] >> 16;
        *(hash+i*4+2) = state->h[i] >> 8;
        *(hash+i*4+3) = state->h[i];
    }
}

ICACHE_FLASH_ATTR static uint32_t rrotate(uint32_t x, uint8_t n) {
    uint32_t left = x >> n;
    uint32_t right =  x & ((1<<(n+1))-1);

    return right<<(32-n) | left;
}

ICACHE_FLASH_ATTR static void sha256_consume(Sha256State *state,
                                             const uint8_t *chunk) {
    int i;
    uint32_t w[64];
    uint32_t s0, s1;
    uint32_t g[8];
    uint32_t S0, S1, ch;
    uint32_t temp1, temp2, maj;

    os_bzero(w, sizeof(w));

    for (i=0; i<16; i++)
        w[i] = (chunk[i*4  ]<<24) | (chunk[i*4+1]<<16) |
               (chunk[i*4+2]<<8)  |  chunk[i*4+3];

    for (i=16; i<64; i++) {
        s0 = rrotate(w[i-15], 7) ^ rrotate(w[i-15], 18) ^ (w[i-15] >> 3);
        s1 = rrotate(w[i-2], 17) ^ rrotate(w[i-2], 19) ^ (w[i-2] >> 10);
        w[i] = w[i-16] + s0 + w[i-7] + s1;
    }

    for (i=0; i<8; i++)
        g[i] = state->h[i];

    for (i=0; i<64; i++) {
        S1 = rrotate(g[4], 6) ^ rrotate(g[4], 11) ^ rrotate(g[4], 25);
        ch = (g[4] & g[5]) ^ (~g[4] & g[6]);
        temp1 = g[7] + S1 + ch + sha256_k[i] + w[i];
        S0 = rrotate(g[0], 2) ^ rrotate(g[0], 13) ^ rrotate(g[0], 22);
        maj = (g[0] & g[1]) ^ (g[0] & g[2]) ^ (g[1] & g[2]);
        temp2 = S0 + maj;

        g[7] = g[6];
        g[6] = g[5];
        g[5] = g[4];
        g[4] = g[3] + temp1;
        g[3] = g[2];
        g[2] = g[1];
        g[1] = g[0];
        g[0] = temp1 + temp2;
    }

    for (i=0; i<8; i++)
        state->h[i] += g[i];
}
