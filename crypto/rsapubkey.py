#!/usr/bin/env python

from pyasn1.codec.der.decoder import decode
from pyasn1.type.namedtype import NamedType, NamedTypes
from pyasn1.type.univ import Integer, Sequence

import base64
import re
import sys

# TODO The OpenSSL RSA public key has a more complex ASN.1 format,
#      so for now just grab the public key components from the private key
#
#      $ openssl asn1parse -in pubkey.pem -i
#          0:d=0  hl=2 l=  92 cons: SEQUENCE          
#          2:d=1  hl=2 l=  13 cons:  SEQUENCE          
#          4:d=2  hl=2 l=   9 prim:   OBJECT            :rsaEncryption
#         15:d=2  hl=2 l=   0 prim:   NULL              
#         17:d=1  hl=2 l=  75 prim:  BIT STRING        
#
#      $ openssl asn1parse -in pubkey.pem -i -strparse 17
#          0:d=0  hl=2 l=  72 cons: SEQUENCE          
#          2:d=1  hl=2 l=  65 prim:  INTEGER           :E3996901...
#         69:d=1  hl=2 l=   3 prim:  INTEGER           :010001
#
#class RsaPubkey(Sequence):
#    componentType = NamedTypes(
#        NamedType('modulus', Integer()),
#        NamedType('publicExponent', Integer())
#    )

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

if __name__ == '__main__':
    if len(sys.argv) != 2:
        print >>sys.stderr, 'usage: %s key.pem' % sys.argv[0]
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

    #
    # Generate the crypto/pubkey.c file
    #

    def tobigint(val, name):
        s = '%x' % val
        if len(s) % 2 == 1:
            s = '0' + s

        # TODO Including ICACHE_RODATA_ATTR causes problems
        print 'const Bigint %s = {' % name
        print '    .bytes = %u,' % (len(s)/2,)
        print '    .bits = 0,'
        print '    .data = {',

        j = 0
        for b in [s[i:i+2] for i in range(0, len(s), 2)][::-1]:
            print '0x%s,' % b,
            j += 1
            if j % 10 == 0:
                print
                print '             ',
        print
        print '            }'
        print '};'
        print

    print '#include "bigint.h"'
    print

    tobigint(privkey['modulus'], 'pubkey_mod')
    tobigint(privkey['publicExponent'], 'pubkey_exp')
