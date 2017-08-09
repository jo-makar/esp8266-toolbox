#include <ip_addr.h> /* Must be included before espconn.h */

#include <c_types.h>
#include <espconn.h>
#include <osapi.h>
#include <spi_flash.h>
#include <upgrade.h>

#include "../crypto/bigint.h"
#include "../crypto/rsa.h"
#include "../crypto/sha256.h"
#include "../log.h"
#include "httpd.h"

typedef struct {
    HttpdClient *client;

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
} FotaState;

static FotaState fotastate;

static void fota_reboot(void *arg);

ICACHE_FLASH_ATTR int httpd_url_fota_bin(HttpdClient *client) {
    HTTPD_IGNORE_POSTDATA

    HTTPD_OUTBUF_APPEND("HTTP/1.1 200 OK\r\n")
    HTTPD_OUTBUF_APPEND("Connection: close\r\n")
    HTTPD_OUTBUF_APPEND("Content-type: text/plain\r\n")
    HTTPD_OUTBUF_APPEND("Content-length: 2\r\n")
    HTTPD_OUTBUF_APPEND("\r\n")
    HTTPD_OUTBUF_PRINTF("%01x\n", system_upgrade_userbin_check())

    if (espconn_secure_send(client->conn, httpd_outbuf, httpd_outbuflen))
        LOG_ERROR(HTTPD, "espconn_secure_send() failed\n")

    return 1;
}

ICACHE_FLASH_ATTR int httpd_url_fota_push(HttpdClient *client) {
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
        fotastate.client = client;

        sha256_init(&fotastate.sha256);

        switch (system_get_flash_size_map()) {
            case FLASH_SIZE_4M_MAP_256_256: size_kb = 4*1024 / 8; break;
            case FLASH_SIZE_8M_MAP_512_512: size_kb = 8*1024 / 8; break;
            case FLASH_SIZE_16M_MAP_1024_1024: size_kb = 16*1024 / 8; break;

            /* TODO Assuming it's the full flash size that's relevant */
            case FLASH_SIZE_16M_MAP_512_512: size_kb = 16*1024 / 8; break;
            case FLASH_SIZE_32M_MAP_512_512: size_kb = 32*1024 / 8; break;
            case FLASH_SIZE_32M_MAP_1024_1024: size_kb = 32*1024 / 8; break;

            case FLASH_SIZE_2M:
            default:
                LOG_ERROR(FOTA, "unsupported flash map (0x%02x)\n")
                goto fail;
                break;
        }

        curaddr = system_get_userbin_addr();
        if (curaddr != 4*1024 && curaddr != (size_kb/2+4)*1024) {
            LOG_ERROR(FOTA, "unexpected userbin addr\n")
            goto fail;
        }

        #define USER1_ADDR 4*1024
        #define USER2_ADDR (size_kb/2+4)*1024
        fotastate.newaddr_beg = fotastate.newaddr_cur =
            curaddr == USER1_ADDR ? USER2_ADDR : USER1_ADDR;

        fotastate.appsize = client->postlen;

        fotastate.secused = 0;

        if (client->state != HTTPD_STATE_POSTDATA) {
            LOG_ERROR(FOTA, "no post data\n")
            goto fail;
        }

        if (client->postlen > (size_kb/2-20) * 1024) {
            LOG_ERROR(FOTA, "max app size exceeded\n")
            goto fail;
        }
    }

    if (fotastate.client != client) {
        LOG_ERROR(FOTA, "multiple client detected\n")
        goto fail;
    }

    /*
     * Process the POST data in chunks
     * (as received from httpd_client_recv_cb)
     */

    if (espconn_recv_hold(client->conn))
        LOG_ERROR(FOTA, "espconn_recv_hold() failed\n")

    LOG_DEBUG(FOTA, "loop begins: bufused=%u\n", client->bufused)

    while (client->bufused > 0 && client->postlen > client->postused) {
        system_soft_wdt_feed();

        len = MIN(client->bufused, client->postlen - client->postused);
        len = MIN(len, SPI_FLASH_SEC_SIZE - fotastate.secused);

        os_memcpy(fotastate.secbuf+fotastate.secused, client->buf, len);
        os_memmove(client->buf, client->buf+len, client->bufused-len);

        client->bufused -= len;
        client->postused += len;
        fotastate.secused += len;

        if (fotastate.secused==SPI_FLASH_SEC_SIZE ||
            client->postused==client->postlen) {

            if (spi_flash_erase_sector(fotastate.newaddr_cur/SPI_FLASH_SEC_SIZE)
                    != SPI_FLASH_RESULT_OK) {
                LOG_ERROR(FOTA, "spi_flash_erase_sector() failed\n")
                goto fail;
            }

            if (spi_flash_write(fotastate.newaddr_cur,
                                (uint32_t *)fotastate.secbuf, SPI_FLASH_SEC_SIZE)
                    != SPI_FLASH_RESULT_OK) {
                LOG_ERROR(FOTA, "spi_flash_write() failed\n")
                goto fail;
            }

            sha256_proc(&fotastate.sha256, fotastate.secbuf, fotastate.secused);

            LOG_INFO(FOTA, "flashed sector 0x%05x\n", fotastate.newaddr_cur)

            fotastate.newaddr_cur += SPI_FLASH_SEC_SIZE;

            os_bzero(fotastate.secbuf, SPI_FLASH_SEC_SIZE);
            fotastate.secused = 0;
        }
    }

    if (espconn_recv_unhold(client->conn))
        LOG_ERROR(FOTA, "espconn_recv_unhold() failed\n")

    if (client->postlen > client->postused)
        return 0;

    sha256_done(&fotastate.sha256, fotastate.hash1);

    HASH_TO_STR(fotastate.hash1, hashstr)
    LOG_INFO(FOTA, "bin hash = %s\n", hashstr)

    /*
     * Read the just written application
     * to verify it was written correctly
     */

    sha256_init(&fotastate.sha256);

    for (addr = fotastate.newaddr_beg;
         addr < fotastate.newaddr_beg + fotastate.appsize;
         addr += SPI_FLASH_SEC_SIZE) {

        system_soft_wdt_feed();

        if (spi_flash_read(addr, (uint32_t *)fotastate.secbuf,
                           SPI_FLASH_SEC_SIZE)
                != SPI_FLASH_RESULT_OK) {
            LOG_ERROR(FOTA, "spi_flash_read() failed\n")
            goto fail;
        }

        sha256_proc(&fotastate.sha256, fotastate.secbuf, 
                    addr + SPI_FLASH_SEC_SIZE >
                            fotastate.newaddr_beg + fotastate.appsize
                        ? (fotastate.appsize % SPI_FLASH_SEC_SIZE)
                        : SPI_FLASH_SEC_SIZE);
    }

    sha256_done(&fotastate.sha256, fotastate.hash2);

    HASH_TO_STR(fotastate.hash2, hashstr)
    LOG_INFO(FOTA, "read hash = %s\n", hashstr)

    if (os_strncmp((char *)fotastate.hash1, (char *)fotastate.hash2, 32) != 0) {
        LOG_ERROR(FOTA, "hash mismatch\n")
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
            LOG_ERROR(FOTA, "no signature\n")
            goto fail;
        }
        start = index((char *)client->url, '?') + 1;

        if (bigint_fromhex(&fotastate.cipher, start) > 0) {
            LOG_ERROR(FOTA, "invalid signature\n")
            goto fail;
        }
        if (rsa_pubkey_decrypt(&fotastate.clear, &fotastate.cipher) > 0) {
            LOG_ERROR(FOTA, "invalid signature\n")
            goto fail;
        }

        bytes = fotastate.clear.bytes + (fotastate.clear.bits>0 ? 1 : 0);

        for (i=0; i<32-bytes; i++)
            fotastate.hash3[i] = 0;
        for (j=bytes-1; j>=0; i++, j--)
            fotastate.hash3[i] = fotastate.clear.data[j];
            
        HASH_TO_STR(fotastate.hash3, hashstr)
        LOG_INFO(FOTA, "sig hash = %s\n", hashstr)

        if (os_strncmp((char *)fotastate.hash1,
                       (char *)fotastate.hash3, 32) != 0) {
            LOG_ERROR(FOTA, "hash mismatch\n")
            goto fail;
        }
    }

    HTTPD_OUTBUF_APPEND("HTTP/1.1 202 Accepted\r\n")
    HTTPD_OUTBUF_APPEND("Connection: close\r\n")
    HTTPD_OUTBUF_APPEND("Content-type: text/html\r\n")
    HTTPD_OUTBUF_APPEND("Content-length: 48\r\n")
    HTTPD_OUTBUF_APPEND("\r\n")
    HTTPD_OUTBUF_APPEND("<html><body><h1>202 Accepted</h1></body></html>\n")

    if (espconn_secure_send(client->conn, httpd_outbuf, httpd_outbuflen))
        LOG_ERROR(HTTPD, "espconn_secure_send() failed\n")

    system_upgrade_flag_set(UPGRADE_FLAG_FINISH);

    /* Set a timer to reboot into the new app after five seconds */
    os_timer_disarm(&fotastate.timer);
    os_timer_setfn(&fotastate.timer, fota_reboot, NULL);
    os_timer_arm(&fotastate.timer, 5000, false);
    
    return 1;

    fail:

    HTTPD_OUTBUF_APPEND("HTTP/1.1 400 Bad Request\r\n")
    HTTPD_OUTBUF_APPEND("Connection: close\r\n")
    HTTPD_OUTBUF_APPEND("Content-type: text/html\r\n")
    HTTPD_OUTBUF_APPEND("Content-length: 51\r\n")
    HTTPD_OUTBUF_APPEND("\r\n")
    HTTPD_OUTBUF_APPEND("<html><body><h1>400 Bad Request</h1></body></html>\n")

    if (espconn_secure_send(client->conn, httpd_outbuf, httpd_outbuflen))
        LOG_ERROR(HTTPD, "espconn_secure_send() failed\n")

    return 1;
}

ICACHE_FLASH_ATTR static void fota_reboot(void *arg) {
    (void)arg;

    LOG_INFO(FOTA, "rebooting\n")
    system_upgrade_reboot();

    /* TODO Apparently sometimes system_upgrade_reboot() doesn't reboot */
    {
        uint16_t i;

        for (i=0; i<3*1000; i++) {
            if (i>0 && i%1000==0)
                system_soft_wdt_feed();
            os_delay_us(1000);
        }

        LOG_WARNING(FOTA, "system_upgrade_reboot() didn't reboot\n")
        system_restart();
    }
}
