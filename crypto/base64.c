#include "../log/log.h"

#include <c_types.h>
#include <ets_sys.h>

static uint8_t encmap(uint8_t c);

ICACHE_FLASH_ATTR int b64_encode(const uint8_t *dec, size_t dlen, uint8_t *enc, size_t elen) {
    size_t i, j;
    uint32_t v;

    if (elen < (dlen/3 + (dlen%3>0 ? 1 : 0)) * 4) {
        ERROR(MAIN, "enc overflow")
        return -1;
    }

    for (i=0, j=0; i<dlen; i+=3) {
        if (i+2 < dlen) {
            v = (dec[i] << 16) | (dec[i+1] << 8) | dec[i+2];
            enc[j++] = encmap((v & 0x00fc0000) >> 18);
            enc[j++] = encmap((v & 0x0003f000) >> 12);
            enc[j++] = encmap((v & 0x00000fc0) >>  6);
            enc[j++] = encmap((v & 0x0000003f));
        }

        else if (i+2 == dlen) {
            v = (dec[i] << 16) | (dec[i+1] << 8);
            enc[j++] = encmap((v & 0x00fc0000) >> 18);
            enc[j++] = encmap((v & 0x0003f000) >> 12);
            enc[j++] = encmap((v & 0x00000fc0) >>  6);
            enc[j++] = '=';
        }

        else if (i+1 == dlen) {
            v = dec[i] << 16;
            enc[j++] = encmap((v & 0x00fc0000) >> 18);
            enc[j++] = encmap((v & 0x0003f000) >> 12);
            enc[j++] = '=';
            enc[j++] = '=';
        }
    }

    return j;
}

ICACHE_FLASH_ATTR uint8_t encmap(uint8_t b) {
    if (/* 0 <= b && */ b <= 25)
        return 65 + b;
    else if (26 <= b && b <= 51)
        return 97 + (b-26);
    else if (52 <= b && b <= 61)
        return 48 + (b-52);
    else if (b == 62)
        return '+';
    else /* if (b == 63) */
        return '/';
}
