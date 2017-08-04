#ifndef RSA_H
#define RSA_H

#include "bigint.h"

extern const Bigint pubkey_mod, pubkey_exp;

int rsa_pubkey_decrypt(Bigint *clear, const Bigint *cipher);

#endif
