#include <ip_addr.h> /* Must be included before espconn.h */

#include <c_types.h>
#include <espconn.h>
#include <upgrade.h>

#include "../crypto/sha256.h"
#include "httpd.h"

typedef struct {
    HttpdClient *client;

    Sha256State sha256;
    uint8_t hash[32];

    uint32_t newaddr_beg;
    uint32_t newaddr_cur;

    #define SECTOR_LEN 4*1024
    uint8_t secbuf[SECTOR_LEN];
    uint16_t secused;

} FotaState;

static FotaState fotastate;

ICACHE_FLASH_ATTR int httpd_url_fota(HttpdClient *client) {
    uint32_t size_kb;
    uint32_t curaddr;
    size_t len;

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
                HTTPD_ERROR("url_fota: unsupported flash map (0x%02x)\n");
                goto fail;
                break;
        }

        curaddr = system_get_userbin_addr();
        if (curaddr != 4*1024 && curaddr != (size_kb/2+4)*1024) {
            HTTPD_ERROR("url_fota: unexpected userbin addr\n")
                goto fail;
        }

        #define USER1_ADDR 4*1024
        #define USER2_ADDR (size_kb/2+4)*1024
        fotastate.newaddr_beg = fotastate.newaddr_cur =
            curaddr == USER1_ADDR ? USER2_ADDR : USER1_ADDR;

        fotastate.secused = 0;

        if (client->state != HTTPD_STATE_POSTDATA) {
            HTTPD_ERROR("url_fota: no post data\n")
            goto fail;
        }

        if (client->postlen > (size_kb/2-20) * 1024) {
            HTTPD_ERROR("url_fota: max app size exceeded\n")
            goto fail;
        }
    }

    if (fotastate.client != client) {
        HTTPD_ERROR("url_fota: multiple client detected\n")
        goto fail;
    }

    /*
     * Process the POST data in chunks
     * (as received from httpd_client_recv_cb)
     */

    if (espconn_recv_hold(client->conn))
        HTTPD_ERROR("url_fota: espconn_recv_hold() failed\n")

    HTTPD_DEBUG("url_fota: loop begins: bufused=%u\n", client->bufused)

    system_soft_wdt_feed();

    while (client->bufused > 0 && client->postlen > client->postused) {
        len = MIN(client->bufused, client->postlen - client->postused);
        len = MIN(len, SECTOR_LEN - fotastate.secused);

        os_memcpy(fotastate.secbuf+fotastate.secused, client->buf, len);
        os_memmove(client->buf, client->buf+len, client->bufused-len);

        client->bufused -= len;
        client->postused += len;
        fotastate.secused += len;

        if (fotastate.secused==SECTOR_LEN || client->postused==client->postlen) {

            /* FIXME STOPPED Write the (partial) sector to flash */

            sha256_proc(&fotastate.sha256, fotastate.secbuf, fotastate.secused);

            HTTPD_INFO("url_fota: flashed sector 0x%05x\n",
                       fotastate.newaddr_cur)

            fotastate.newaddr_cur += SECTOR_LEN;

            os_bzero(fotastate.secbuf, SECTOR_LEN);
            fotastate.secused = 0;
        }
    }

    if (espconn_recv_unhold(client->conn))
        HTTPD_ERROR("url_fota: espconn_recv_unhold() failed\n")

    if (client->postlen > client->postused)
        return 0;

    sha256_done(&fotastate.sha256, fotastate.hash);

    #if HTTPD_LOG_LEVEL <= LEVEL_INFO
    {
        uint32_t t = system_get_time();
        uint8_t i;

        os_printf("%u.%03u: info: %s:%u: url_fota: sha256=",
                  t/1000000, (t%1000000)/1000, __FILE__, __LINE__);
        for (i=0; i<32; i++)
            os_printf("%02x", fotastate.hash[i]);
        os_printf("\n");
    }
    #endif

    HTTPD_OUTBUF_APPEND("HTTP/1.1 202 Accepted\r\n")
    HTTPD_OUTBUF_APPEND("Connection: close\r\n")
    HTTPD_OUTBUF_APPEND("Content-type: text/html\r\n")
    HTTPD_OUTBUF_APPEND("Content-length: 47\r\n")
    HTTPD_OUTBUF_APPEND("\r\n")
    HTTPD_OUTBUF_APPEND("<html><body><h1>202 Accepted</h1></body></html>")

    if (espconn_send(client->conn, httpd_outbuf, httpd_outbuflen))
        HTTPD_ERROR("url_fota: espconn_send() failed\n")

    /* FIXME Set upgrade flag & reboot */

    return 1;

    fail:

    HTTPD_OUTBUF_APPEND("HTTP/1.1 400 Bad Request\r\n")
    HTTPD_OUTBUF_APPEND("Connection: close\r\n")
    HTTPD_OUTBUF_APPEND("Content-type: text/html\r\n")
    HTTPD_OUTBUF_APPEND("Content-length: 50\r\n")
    HTTPD_OUTBUF_APPEND("\r\n")
    HTTPD_OUTBUF_APPEND("<html><body><h1>400 Bad Request</h1></body></html>")

    if (espconn_send(client->conn, httpd_outbuf, httpd_outbuflen))
        HTTPD_ERROR("url_fota: espconn_send() failed\n")

    return 1;
}
