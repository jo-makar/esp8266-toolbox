#ifndef CRYPTO_MD5_H
#define CRYPTO_MD5_H

#include <c_types.h>

typedef struct {
    uint8_t buf[64], buflen;
    uint32_t totallen;
    uint32_t h[4];
} Md5State;

#define MD5_BLOCKSIZE 64
#define MD5_HASHLEN   16

void md5_init(Md5State *state);
void md5_proc(Md5State *state, const uint8_t *buf, size_t len);
void md5_done(Md5State *state, uint8_t *hash);

void md5(const uint8_t *buf, size_t len, uint8_t *hash);

#endif
