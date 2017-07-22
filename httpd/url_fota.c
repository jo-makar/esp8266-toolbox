#include <ip_addr.h> /* Must be included before espconn.h */

#include <c_types.h>
#include <espconn.h>
#include <mem.h>
#include <upgrade.h>

#include "httpd.h"

ICACHE_FLASH_ATTR int httpd_url_fota(HttpdClient *client) {
    uint32_t size_kb;
    uint32_t curaddr, newaddr;
    uint8_t *buf = NULL;

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
    newaddr = curaddr == USER1_ADDR ? USER2_ADDR : USER1_ADDR;

    #define SECTOR_LEN 4*1024
    if ((buf = os_malloc(SECTOR_LEN)) == NULL) {
        HTTPD_ERROR("url_fota: os_malloc failed\n")
        goto fail;
    }

    if (client->state != HTTPD_STATE_POSTDATA) {
        HTTPD_ERROR("url_fota: no post data\n")
        goto fail;
    }

    if (client->postlen > (size_kb/2-20) * 1024) {
        HTTPD_ERROR("url_fota: max app size exceeded\n")
        goto fail;
    }

    HTTPD_IGNORE_POSTDATA
    /* FIXME STOPPED Read and process the post data in 4KB chunks */
    /* FIXME Must use espconn_recv_hold, unhold to block sender here */
    /* FIXME Also kick the watchdog each iteration? */

    HTTPD_OUTBUF_APPEND("HTTP/1.1 202 Accepted\r\n")
    HTTPD_OUTBUF_APPEND("Connection: close\r\n")
    HTTPD_OUTBUF_APPEND("Content-type: text/html\r\n")
    HTTPD_OUTBUF_APPEND("Content-length: 47\r\n")
    HTTPD_OUTBUF_APPEND("\r\n")
    HTTPD_OUTBUF_APPEND("<html><body><h1>202 Accepted</h1></body></html>")

    if (espconn_send(client->conn, httpd_outbuf, httpd_outbuflen))
        HTTPD_ERROR("url_fota: espconn_send() failed\n")

    return 1;

    fail:

    if (buf != NULL)
        os_free(buf);

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
