#include <ip_addr.h>
#include <espconn.h>
#include <osapi.h>
#include <spi_flash.h>
#include <upgrade.h>

#include "../crypto/bigint.h"
#include "../crypto/rsa.h"
#include "../crypto/sha256.h"
#include "../log/log.h"
#include "private.h"

typedef struct {
    HttpClient *client;

    Sha256State sha256;
    uint8_t hash1[32], hash2[32];

    uint32_t newaddr_beg;
    uint32_t newaddr_cur;
    uint32_t appsize;

    uint8_t secbuf[SPI_FLASH_SEC_SIZE];
    uint16_t secused;

    os_timer_t timer;

    Bigint cipher, clear;
    uint8_t hash3[32];
} OtaState;

static OtaState otastate;

static void ota_reboot(void *arg);

ICACHE_FLASH_ATTR int http_url_ota_bin(HttpClient *client) {
    HTTP_IGNORE_POSTDATA

    uint8_t text = 0;

    if (os_strstr((char *)client->url, "?text") != NULL)
        text = 1;

    if (text) {
        HTTP_OUTBUF_APPEND("HTTP/1.1 200 OK\r\n")
        HTTP_OUTBUF_APPEND("Connection: close\r\n")
        HTTP_OUTBUF_APPEND("Content-type: text/plain\r\n")
        HTTP_OUTBUF_APPEND("Content-length: 2\r\n")
        HTTP_OUTBUF_APPEND("\r\n")
        HTTP_OUTBUF_PRINTF("%01x\n", system_upgrade_userbin_check())
    } else {
        HTTP_OUTBUF_APPEND("HTTP/1.1 200 OK\r\n")
        HTTP_OUTBUF_APPEND("Connection: close\r\n")
        HTTP_OUTBUF_APPEND("Content-type: text/html\r\n")
        HTTP_OUTBUF_APPEND("Content-length: 37\r\n")
        HTTP_OUTBUF_APPEND("\r\n")
        HTTP_OUTBUF_PRINTF("<html><body><h1>%01x</h1></body></html>\n",
                           system_upgrade_userbin_check())
    }

    if (espconn_send(client->conn, http_outbuf, http_outbuflen))
        ERROR(HTTP, "espconn_send() failed")

    return 1;
}

ICACHE_FLASH_ATTR int http_url_ota_push(HttpClient *client) {
    uint32_t size_kb;
    uint32_t curaddr;
    size_t len;
    uint32_t addr;
    char hashstr[64+1];

    #define HASH_TO_STR(h, s) { \
        uint8_t i; \
        \
        for (i=0; i<32; i++) \
            os_snprintf(s+i*2, 3, "%02x", h[i]); \
        \
        s[64] = 0; \
    }

    if (client->postused == 0) {
        otastate.client = client;

        sha256_init(&otastate.sha256);

        switch (system_get_flash_size_map()) {
            case FLASH_SIZE_4M_MAP_256_256: size_kb = 4*1024 / 8; break;
            case FLASH_SIZE_8M_MAP_512_512: size_kb = 8*1024 / 8; break;
            case FLASH_SIZE_16M_MAP_1024_1024: size_kb = 16*1024 / 8; break;

            /* Assuming it's the full flash size that's relevant */
            case FLASH_SIZE_16M_MAP_512_512: size_kb = 16*1024 / 8; break;
            case FLASH_SIZE_32M_MAP_512_512: size_kb = 32*1024 / 8; break;
            case FLASH_SIZE_32M_MAP_1024_1024: size_kb = 32*1024 / 8; break;

            case FLASH_SIZE_2M:
            default:
                ERROR(OTA, "unsupported flash map (0x%02x)")
                goto fail;
                break;
        }

        curaddr = system_get_userbin_addr();
        if (curaddr != 4*1024 && curaddr != (size_kb/2+4)*1024) {
            ERROR(OTA, "unexpected userbin addr")
            goto fail;
        }

        #define USER1_ADDR 4*1024
        #define USER2_ADDR (size_kb/2+4)*1024
        otastate.newaddr_beg = otastate.newaddr_cur =
            curaddr == USER1_ADDR ? USER2_ADDR : USER1_ADDR;

        otastate.appsize = client->postlen;

        otastate.secused = 0;

        if (client->state != HTTP_STATE_POSTDATA) {
            ERROR(OTA, "no post data")
            goto fail;
        }

        if (client->postlen > (size_kb/2-20) * 1024) {
            ERROR(OTA, "max app size exceeded")
            goto fail;
        }
    }

    if (otastate.client != client) {
        ERROR(OTA, "multiple client detected")
        goto fail;
    }

    /*
     * Process the POST data in chunks
     * (as received from http_client_recv_cb)
     */

    if (espconn_recv_hold(client->conn))
        ERROR(OTA, "espconn_recv_hold() failed")

    DEBUG(OTA, "loop begins: bufused=%u", client->bufused)

    while (client->bufused > 0 && client->postlen > client->postused) {
        system_soft_wdt_feed();

        len = MIN(client->bufused, client->postlen - client->postused);
        len = MIN(len, SPI_FLASH_SEC_SIZE - otastate.secused);

        os_memcpy(otastate.secbuf+otastate.secused, client->buf, len);
        os_memmove(client->buf, client->buf+len, client->bufused-len);

        client->bufused -= len;
        client->postused += len;
        otastate.secused += len;

        if (otastate.secused==SPI_FLASH_SEC_SIZE ||
            client->postused==client->postlen) {

            if (spi_flash_erase_sector(otastate.newaddr_cur/SPI_FLASH_SEC_SIZE)
                    != SPI_FLASH_RESULT_OK) {
                ERROR(OTA, "spi_flash_erase_sector() failed")
                goto fail;
            }

            if (spi_flash_write(otastate.newaddr_cur,
                                (uint32_t *)otastate.secbuf, SPI_FLASH_SEC_SIZE)
                    != SPI_FLASH_RESULT_OK) {
                ERROR(OTA, "spi_flash_write() failed")
                goto fail;
            }

            sha256_proc(&otastate.sha256, otastate.secbuf, otastate.secused);

            DEBUG(OTA, "flashed sector 0x%05x", otastate.newaddr_cur)

            otastate.newaddr_cur += SPI_FLASH_SEC_SIZE;

            os_bzero(otastate.secbuf, SPI_FLASH_SEC_SIZE);
            otastate.secused = 0;
        }
    }

    if (espconn_recv_unhold(client->conn))
        ERROR(OTA, "espconn_recv_unhold() failed")

    if (client->postlen > client->postused)
        return 0;

    sha256_done(&otastate.sha256, otastate.hash1);

    HASH_TO_STR(otastate.hash1, hashstr)
    INFO(OTA, "bin hash = %s", hashstr)

    /*
     * Read the just written application
     * to verify it was written correctly
     */

    sha256_init(&otastate.sha256);

    for (addr = otastate.newaddr_beg;
         addr < otastate.newaddr_beg + otastate.appsize;
         addr += SPI_FLASH_SEC_SIZE) {

        system_soft_wdt_feed();

        if (spi_flash_read(addr, (uint32_t *)otastate.secbuf,
                           SPI_FLASH_SEC_SIZE)
                != SPI_FLASH_RESULT_OK) {
            ERROR(OTA, "spi_flash_read() failed")
            goto fail;
        }

        sha256_proc(&otastate.sha256, otastate.secbuf, 
                    addr + SPI_FLASH_SEC_SIZE >
                            otastate.newaddr_beg + otastate.appsize
                        ? (otastate.appsize % SPI_FLASH_SEC_SIZE)
                        : SPI_FLASH_SEC_SIZE);
    }

    sha256_done(&otastate.sha256, otastate.hash2);

    HASH_TO_STR(otastate.hash2, hashstr)
    INFO(OTA, "read hash = %s", hashstr)

    if (os_strncmp((char *)otastate.hash1, (char *)otastate.hash2, 32) != 0) {
        ERROR(OTA, "hash mismatch")
        goto fail;
    }

    /*
     * Verify the signature
     */

    {
        char *start;
        int16_t bytes;
        int16_t i, j;

        if (index((char *)client->url, '?') == NULL) {
            ERROR(OTA, "no signature")
            goto fail;
        }
        start = index((char *)client->url, '?') + 1;

        if (bigint_fromhex(&otastate.cipher, start) > 0) {
            ERROR(OTA, "invalid signature")
            goto fail;
        }
        if (rsa_pubkey_decrypt(&otastate.clear, &otastate.cipher) > 0) {
            ERROR(OTA, "invalid signature")
            goto fail;
        }

        bytes = otastate.clear.bytes + (otastate.clear.bits>0 ? 1 : 0);

        for (i=0; i<32-bytes; i++)
            otastate.hash3[i] = 0;
        for (j=bytes-1; j>=0; i++, j--)
            otastate.hash3[i] = otastate.clear.data[j];
            
        HASH_TO_STR(otastate.hash3, hashstr)
        INFO(OTA, "sig hash = %s", hashstr)

        if (os_strncmp((char *)otastate.hash1,
                       (char *)otastate.hash3, 32) != 0) {
            ERROR(OTA, "hash mismatch")
            goto fail;
        }
    }

    HTTP_OUTBUF_APPEND("HTTP/1.1 202 Accepted\r\n")
    HTTP_OUTBUF_APPEND("Connection: close\r\n")
    HTTP_OUTBUF_APPEND("Content-type: text/html\r\n")
    HTTP_OUTBUF_APPEND("Content-length: 48\r\n")
    HTTP_OUTBUF_APPEND("\r\n")
    HTTP_OUTBUF_APPEND("<html><body><h1>202 Accepted</h1></body></html>\n")

    if (espconn_send(client->conn, http_outbuf, http_outbuflen))
        ERROR(HTTP, "espconn_send() failed")

    system_upgrade_flag_set(UPGRADE_FLAG_FINISH);

    os_timer_disarm(&otastate.timer);
    os_timer_setfn(&otastate.timer, ota_reboot, NULL);
    os_timer_arm(&otastate.timer, 5000, false);
    
    return 1;

    fail:

    HTTP_OUTBUF_APPEND("HTTP/1.1 400 Bad Request\r\n")
    HTTP_OUTBUF_APPEND("Connection: close\r\n")
    HTTP_OUTBUF_APPEND("Content-type: text/html\r\n")
    HTTP_OUTBUF_APPEND("Content-length: 51\r\n")
    HTTP_OUTBUF_APPEND("\r\n")
    HTTP_OUTBUF_APPEND("<html><body><h1>400 Bad Request</h1></body></html>\n")

    if (espconn_send(client->conn, http_outbuf, http_outbuflen))
        ERROR(HTTP, "espconn_send() failed")

    return 1;
}

ICACHE_FLASH_ATTR static void ota_reboot(void *arg) {
    (void)arg;

    INFO(OTA, "rebooting")
    system_upgrade_reboot();
}
