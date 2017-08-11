#!/usr/bin/env python

from itertools import permutations

if __name__ == '__main__':
    def printbytes(x):
        s = '%x' % x
        p = '0' if len(s) % 2 == 1 else ''
        print p + s

    printbytes(0xabcd + 0xcdab)
    printbytes(0xcdab + 0xabcd)

    printbytes(0xabcd * 0xcdab)
    printbytes(0xcdab * 0xabcd)

    printbytes(0xabcd / 0xcdab)
    printbytes(0xabcd % 0xcdab)
    printbytes(0xcdab / 0xabcd)
    printbytes(0xcdab % 0xabcd)

    a, b, c = 0xabcd, 0xcdab, 0x55aa
    for f, s, t in permutations([a, b, c]):
        printbytes((f ** s) % t)
