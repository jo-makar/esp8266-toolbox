#ifndef CRYPTO_SHA256_H
#define CRYPTO_SHA256_H

#include <c_types.h>

typedef struct {
    uint8_t buf[64], buflen;
    uint32_t totallen;
    uint32_t h[8];
} Sha256State;

void sha256_init(Sha256State *state);
void sha256_proc(Sha256State *state, const uint8_t *buf, size_t len);
void sha256_done(Sha256State *state, uint8_t *hash);

#endif
