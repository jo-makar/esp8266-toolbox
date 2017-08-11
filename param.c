#include "log/log.h"
#include "missing-decls.h"
#include "param.h"

#include <c_types.h>
#include <osapi.h>
#include <spi_flash.h>
#include <user_interface.h>

static const char PARAM_MAGIC[] = {0xca, 0xfe, 0xba, 0xbe};

static uint8_t param_buf[1024];

ICACHE_FLASH_ATTR int param_store(uint16_t offset, const uint8_t *val, uint16_t len) {
    uint32_t size_kb;
    uint32_t addr;

    /*
     * The first partition of flash has nothing special at the end,
     * the second ends with sys param data (last 16KB or 4 sectors).
     * Hence the last four sectors of the first partition are available.
     */

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
            ERROR(MAIN, "unsupported flash map (0x%02x)")
            return 1;
            break;
    }

    addr = (size_kb/2 - 4*3) * 1024;

    if (sizeof(param_buf) < sizeof(PARAM_MAGIC) + len) {
        ERROR(MAIN, "param_buf overflow")
        return 1;
    }

    if (!system_param_load(addr/SPI_FLASH_SEC_SIZE, 0, param_buf, sizeof(param_buf)))
        return 1;

    os_memcpy(param_buf + offset, PARAM_MAGIC, sizeof(PARAM_MAGIC));
    os_memcpy(param_buf + offset + sizeof(PARAM_MAGIC), val, len);

    if (!system_param_save_with_protect(addr/SPI_FLASH_SEC_SIZE,
                                        param_buf, sizeof(param_buf))) {
        ERROR(MAIN, "system_param_save_with_protect() failed")
        return 1;
    }

    return 0;
}

ICACHE_FLASH_ATTR int param_retrieve(uint16_t offset, uint8_t *val, uint16_t len) {
    uint32_t size_kb;
    uint32_t addr;

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
            ERROR(MAIN, "unsupported flash map (0x%02x)")
            return 1;
            break;
    }

    addr = (size_kb/2 - 4*3) * 1024;

    if (!system_param_load(addr/SPI_FLASH_SEC_SIZE, offset, param_buf, sizeof(PARAM_MAGIC) + len)) {
        ERROR(MAIN, "system_param_load() failed")
        return 1;
    }

    if (os_memcmp(param_buf, PARAM_MAGIC, sizeof(PARAM_MAGIC) != 0)) {
        WARNING(MAIN, "undefined/corrupted param flash")
        return 1;
    }

    os_memcpy(val, param_buf+sizeof(PARAM_MAGIC), len);

    return 0;
}
