#include "../log/log.h"

#include <c_types.h>
#include <ets_sys.h>

static uint8_t decmap(uint8_t c);
static uint8_t encmap(uint8_t c);

ICACHE_FLASH_ATTR int b64decode(const uint8_t *enc, size_t elen, uint8_t *dec, size_t dlen) {
    size_t i, j;
    uint8_t last;
    uint32_t v;

    if (elen % 4 != 0) {
        ERROR(MAIN, "elen % 4 != 0")
        return -2;
    }

    for (i=0; i<elen; i++)
      if (!((65 <= enc[i] && enc[i] <= 90)  ||
            (97 <= enc[i] && enc[i] <= 122) ||
            (48 <= enc[i] && enc[i] <= 57)  ||
            enc[i] == '+' || enc[i] == '/' || enc[i] == '='))
      {
          ERROR(MAIN, "bad input")
          return -2;
      }

    if (elen/4*3 > dlen) {
        ERROR(MAIN, "dec overflow")
        return -1;
    }

    for (i=0, j=0; i<elen; i+=4) {
        last = i+4 >= elen;

        if (last && enc[i+2] == '=' && enc[i+3] == '=') {
            v = (decmap(enc[i  ])<<18) |
                (decmap(enc[i+1])<<12);

            dec[j++] = (v & 0x00ff0000) >> 16;
        }

        else if (last && enc[i+3] == '=') {
            v = (decmap(enc[i  ])<<18) |
                (decmap(enc[i+1])<<12) |
                (decmap(enc[i+2])<< 6);

            dec[j++] = (v & 0x00ff0000) >> 16;
            dec[j++] = (v & 0x0000ff00) >>  8;
        }

        else {
            v = (decmap(enc[i  ])<<18) |
                (decmap(enc[i+1])<<12) |
                (decmap(enc[i+2])<< 6) |
                decmap(enc[i+3]);

            dec[j++] = (v & 0x00ff0000) >> 16;
            dec[j++] = (v & 0x0000ff00) >>  8;
            dec[j++] =  v & 0x000000ff;
        }
  }

  return 0;
}

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

ICACHE_FLASH_ATTR uint8_t decmap(uint8_t c) {
  if (65 <= c && c <= 90)
    return c - 65;
  else if (97 <= c && c <= 122)
    return (c - 97) + 26;
  else if (48 <= c && c <= 57)
    return (c - 48) + 52;
  else if (c == '+')
    return 62;
  else if (c == '/')
    return 63;

  /* Should never happen */
  else
    return 0;
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
