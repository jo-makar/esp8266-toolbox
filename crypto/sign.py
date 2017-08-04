#!/usr/bin/env python

# TODO Prefer Openssl utilties but having difficulty specifying no padding:
#          openssl pkeyutl -in input -inkey privkey.pem
#                          -sign -pkeyopt digest:sha256
#                          -pkeyopt rsa_padding_mode:none

from pyasn1.codec.der.decoder import decode
from pyasn1.type.namedtype import NamedType, NamedTypes
from pyasn1.type.univ import Integer, Sequence

import base64
import hashlib
import re
import sys

class RsaPrivkey(Sequence):
    componentType = NamedTypes(
         NamedType('version', Integer()),
         NamedType('modulus', Integer()),
         NamedType('publicExponent', Integer()),
         NamedType('privateExponent', Integer()),
         NamedType('prime1', Integer()),
         NamedType('prime2', Integer()),
         NamedType('exponent1', Integer()),
         NamedType('exponent2', Integer()),
         NamedType('coefficient', Integer())
    )

# Calculate (a^b) mod c quickly
# using (d*e) mod f = ((d mod f) * (e mod f)) mod f
# and   (g^h) mod i = ((g mod i)^h) mod i
# via induction: (a^n) mod c = (((a^(n-1)) mod c) * ((a^(n-1)) mod c)) mod c
def expmod(a, b, c):
    assert b > 0

    # Remove the prepended 0b
    bs = bin(b)[2:]

    xs = [a % c]
    while len(xs) < len(bs):
        xs.append((xs[-1] * xs[-1]) % c)

    rv = 1
    for p in range(len(bs)):
        if bs[p] == '1':
            rv *= xs[len(xs)-1 - p]
    return rv % c

if __name__ == '__main__':
    if len(sys.argv) != 3:
        print >>sys.stderr, 'usage: %s key.pem file' % sys.argv[0]
        sys.exit(1)

    b64_encoded = ''
    with open(sys.argv[1]) as pemfile:
        assert re.match('[-]+BEGIN RSA PRIVATE KEY[-]+', pemfile.readline())
        for line in pemfile:
            if re.match('[-]+END RSA PRIVATE KEY[-]+', line):
                break
            b64_encoded += line.strip()
    der_encoded = base64.decodestring(b64_encoded)

    privkey, rest = decode(der_encoded, asn1Spec=RsaPrivkey())
    assert rest == ''

    sha256 = hashlib.sha256()
    with open(sys.argv[2], 'rb') as infile:
        while True:
            chunk = infile.read(4096)
            if chunk == '':
                break
            sha256.update(chunk)

    cipher = expmod(int(sha256.hexdigest(), 16),
                    privkey['privateExponent'],
                    privkey['modulus'])

    signed = '%x' % cipher
    signed = ('0' * (512/4 - len(signed))) + signed

    with open(sys.argv[2] + '.sig', 'wb') as sigfile:
        for c in [signed[i:i+2] for i in range(0, len(signed), 2)]:
            sigfile.write(chr(int(c, 16)))
