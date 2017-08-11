#!/usr/bin/env python

from itertools import permutations

if __name__ == '__main__':
    print '#include <stdio.h>'
    print ''
    print '#include "bigint.h"'
    print ''
    print 'int main(int argc, char *argv[]) {'
    print '    (void)argc;'
    print '    (void)argv;'
    print ''
    print '    Bigint x, y, a, b, c;'
    print ''
    print '    if (bigint_fromhex(&a, "abcd")) return 1;'
    print '    if (bigint_fromhex(&b, "cdab")) return 1;'
    print ''
    print '    if (bigint_add(&x, &a, &b)) return 1;'
    print '    bigint_print(&x, 0); printf("\\n");'
    print '    if (bigint_add(&x, &b, &a)) return 1;'
    print '    bigint_print(&x, 0); printf("\\n");'
    print ''
    print '    if (bigint_mul(&x, &a, &b)) return 1;'
    print '    bigint_print(&x, 0); printf("\\n");'
    print '    if (bigint_mul(&x, &b, &a)) return 1;'
    print '    bigint_print(&x, 0); printf("\\n");'
    print ''
    print '    if (bigint_div(&x, &y, &a, &b)) return 1;'
    print '    bigint_print(&x, 0); printf("\\n");'
    print '    bigint_print(&y, 0); printf("\\n");'
    print '    if (bigint_div(&x, &y, &b, &a)) return 1;'
    print '    bigint_print(&x, 0); printf("\\n");'
    print '    bigint_print(&y, 0); printf("\\n");'
    print ''

    print '    if (bigint_fromhex(&a, "abcd")) return 1;'
    print '    if (bigint_fromhex(&b, "cdab")) return 1;'
    print '    if (bigint_fromhex(&c, "55aa")) return 1;'
    for f, s, t in permutations(['&a', '&b', '&c']):
        print '    if (bigint_expmod(&x, %s, %s, %s)) return 1;' % (f, s, t)
        print '    bigint_print(&x, 0); printf("\\n");'

    print ''
    print '    return 0;'
    print '}'
