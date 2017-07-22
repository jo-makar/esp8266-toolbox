#include <c_types.h>
#include <upgrade.h>

#include "httpd.h"

ICACHE_FLASH_ATTR int httpd_url_fota(HttpdClient *client) {
    uint32_t user1, user2;
    uint32_t size_kb;

    HTTPD_IGNORE_POSTDATA

    /* FIXME Read and process the post data */

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
            /* TODO Does this flash size map support partitioning? */
            HTTPD_ERROR("url_fota: unsupported flash map (0x%02x)\n");
            goto fail;
            break;
    }

    os_printf("system_upgrade_userbin_check = %d\n", system_upgrade_userbin_check());
    os_printf("system_get_userbin_addr = %08x\n", system_get_userbin_addr());
    os_printf("system_get_flash_size_map = %d\n", system_get_flash_size_map());
    os_printf("system_upgrade_flag_check = %d\n", system_upgrade_flag_check());

    HTTPD_OUTBUF_APPEND("HTTP/1.1 202 Accepted\r\n")
    HTTPD_OUTBUF_APPEND("Connection: close\r\n")
    HTTPD_OUTBUF_APPEND("Content-type: text/html\r\n")
    HTTPD_OUTBUF_APPEND("Content-length: 26\r\n")
    HTTPD_OUTBUF_APPEND("\r\n")
    HTTPD_OUTBUF_APPEND("<html><body></body></html>")
    /* FIXME Include fw info and a msg that a reboot is imminent */

    return 0;

    fail:

    HTTPD_OUTBUF_APPEND("HTTP/1.1 400 Bad Request\r\n")
    HTTPD_OUTBUF_APPEND("Connection: close\r\n")
    HTTPD_OUTBUF_APPEND("Content-type: text/html\r\n")
    HTTPD_OUTBUF_APPEND("Content-length: 26\r\n")
    HTTPD_OUTBUF_APPEND("\r\n")
    HTTPD_OUTBUF_APPEND("<html><body></body></html>")
    /* FIXME Include reason fw was rejected */

    return 0;
}
