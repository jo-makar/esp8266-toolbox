#ifndef CRYPTO_BASE64_H
#define CRYPTO_BASE64_H

#include <c_types.h>

int b64_decode(const uint8_t *enc, size_t elen, uint8_t *dec, size_t dlen);
int b64_encode(const uint8_t *dec, size_t dlen, uint8_t *enc, size_t elen);

#endif
