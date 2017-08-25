#include "../i2c/i2c_master.h"
#include "../log/log.h"
#include "tsl2561.h"

#include <ets_sys.h>

#define I2C_ADDR_PINFLOAT 0x39

#define I2C_ADDR I2C_ADDR_PINFLOAT

#define REG_CONTROL  0x00
#define REG_TIMING   0x01
#define REG_ID       0x0a
#define REG_DATA0LOW 0x0c
#define REG_DATA1LOW 0x0e

static int tsl2561_readbyte(uint8_t reg, uint8_t *val);
static int tsl2561_readword(uint8_t reg, uint16_t *val);
static int tsl2561_writebyte(uint8_t reg, uint8_t val);

static uint16_t calc_lux(uint8_t gain, uint8_t time, uint8_t type,
                         uint16_t data0, uint16_t data1);

ICACHE_FLASH_ATTR int32_t tsl2561_lux() {
    uint8_t id;
    uint8_t part;
    uint16_t i;
    uint16_t data0, data1;

    if (tsl2561_readbyte(REG_ID, &id)) {
        ERROR(DRIVER, "bad/no response")
        return -1;
    }
    DEBUG(DRIVER, "ID = %02x", id)

    part = id >> 4;
    if (!(part == 1 || part == 5)) {
        ERROR(DRIVER, "unexpected part")
        return -1;
    }
    if (part == 1)
        DEBUG(DRIVER, "TSL2561CS")
    else
        DEBUG(DRIVER, "TSL2561T/FN/CL")

    /* Power on */
    if (tsl2561_writebyte(REG_CONTROL, 0x03)) {
        ERROR(DRIVER, "bad/no response")
        return -1;
    }

    /* 16x gain, 402 ms integration */
    if (tsl2561_writebyte(REG_TIMING, 0x12)) {
        ERROR(DRIVER, "bad/no response")
        return -1;
    }

    for (i=0; i<500; i++) {
        if (i>0 && i%10==0)
            system_soft_wdt_feed();
        os_delay_us(1000);
    }

    if (tsl2561_readword(REG_DATA0LOW, &data0)) {
        ERROR(DRIVER, "bad/no response")
        return -1;
    }
    DEBUG(DRIVER, "data0 = %04x", data0)

    if (tsl2561_readword(REG_DATA1LOW, &data1)) {
        ERROR(DRIVER, "bad/no response")
        return -1;
    }
    DEBUG(DRIVER, "data1 = %04x", data1)

    /* Power off */
    if (tsl2561_writebyte(REG_CONTROL, 0x00)) {
        ERROR(DRIVER, "bad/no response")
        return -1;
    }

    return calc_lux(1, 2, part == 5 ? 0 : 1, data0, data1);
}

ICACHE_FLASH_ATTR int tsl2561_readbyte(uint8_t reg, uint8_t *val) {
    if (reg > 0x0f)
        return -1;

    i2c_master_start();
    i2c_master_writeByte(I2C_ADDR << 1);
    if (!i2c_master_checkAck()) {
        i2c_master_stop();
        return -1;
    }

    i2c_master_writeByte(0x80 | reg);
    if (!i2c_master_checkAck()) {
        i2c_master_stop();
        return -1;
    }

    i2c_master_start();
    i2c_master_writeByte((I2C_ADDR << 1) | 0x01);
    if (!i2c_master_checkAck()) {
        i2c_master_stop();
        return -1;
    }

    *val = i2c_master_readByte();
    i2c_master_send_nack();

    i2c_master_stop();

    return 0;
}

ICACHE_FLASH_ATTR int tsl2561_readword(uint8_t reg, uint16_t *val) {
    uint8_t low, high;

    if (reg > 0x0f)
        return -1;

    i2c_master_start();
    i2c_master_writeByte(I2C_ADDR << 1);
    if (!i2c_master_checkAck()) {
        i2c_master_stop();
        return -1;
    }

    i2c_master_writeByte(0x80 | reg);
    if (!i2c_master_checkAck()) {
        i2c_master_stop();
        return -1;
    }

    i2c_master_start();
    i2c_master_writeByte((I2C_ADDR << 1) | 0x01);
    if (!i2c_master_checkAck()) {
        i2c_master_stop();
        return -1;
    }

    low = i2c_master_readByte();
    i2c_master_send_ack();

    high = i2c_master_readByte();
    i2c_master_send_nack();

    i2c_master_stop();

    *val = (high<<8) | low;
    return 0;
}

ICACHE_FLASH_ATTR int tsl2561_writebyte(uint8_t reg, uint8_t val) {
    if (reg > 0x0f)
        return -1;

    i2c_master_start();
    i2c_master_writeByte(I2C_ADDR << 1);
    if (!i2c_master_checkAck()) {
        i2c_master_stop();
        return -1;
    }

    i2c_master_writeByte(0x80 | reg);
    if (!i2c_master_checkAck()) {
        i2c_master_stop();
        return -1;
    }

    i2c_master_writeByte(val);
    if (!i2c_master_checkAck()) {
        i2c_master_stop();
        return -1;
    }

    i2c_master_stop();

    return 0;
}

/*
 * Calculate the approximate illuminance in lux given the raw channel values
 *
 *               Gain: 1x => 0, 16x => 1
 * (Integration) time: 13.7 ms => 0, 101 ms => 1, 402 ms => 2, manual => 3
 *               Type: T/FN/CL => 0, CS => 1
 */
ICACHE_FLASH_ATTR uint16_t calc_lux(uint8_t gain, uint8_t time, uint8_t type,
                                    uint16_t raw0, uint16_t raw1) {
    uint32_t scale;
    uint32_t data0, data1;
    uint32_t ratio;
    uint16_t b, m;
    uint32_t rv;

    switch (time) {
        case 0:  scale = 0x7517; break;
        case 1:  scale = 0x0fe7; break;

        case 2:
        default: scale = 1<<10;  break;
    }

    if (gain == 0)
        scale <<= 4;

    data0 = (raw0 * scale) >> 10;
    data1 = (raw1 * scale) >> 10;

    ratio = 0;
    if (data0 != 0)
        ratio = (data1 << 10) / data0; 
    ratio = (ratio + 1) >> 1;

    if (type == 0) {
        if (ratio <= 0x0040) {
            b = 0x01f2;
            m = 0x01be;
        } else if (ratio <= 0x0080) {
            b = 0x0214;
            m = 0x02d1;
        } else if (ratio <= 0x00c0) {
            b = 0x023f;
            m = 0x037b;
        } else if (ratio <= 0x0100) {
            b = 0x0270;
            m = 0x03fe;
        } else if (ratio <= 0x0138) {
            b = 0x016f;
            m = 0x01fc;
        } else if (ratio <= 0x019a) {
            b = 0x00d2;
            m = 0x00fb;
        } else if (ratio <= 0x029a) {
            b = 0x0018;
            m = 0x0012;
        } else {
            b = 0x0000;
            m = 0x0000;
        }
    }

    else {
        if (ratio <= 0x0043) {
            b = 0x0204;
            m = 0x01ad;
        } else if (ratio <= 0x0085) {
            b = 0x0228;
            m = 0x02c1;
        } else if (ratio <= 0x00c8) {
            b = 0x0253;
            m = 0x0363;
        } else if (ratio <= 0x010a) {
            b = 0x0282;
            m = 0x03df;
        } else if (ratio <= 0x014d) {
            b = 0x0177;
            m = 0x01dd;
        } else if (ratio <= 0x019a) {
            b = 0x0101;
            m = 0x0127;
        } else if (ratio <= 0x029a) {
            b = 0x0037;
            m = 0x002b;
        } else {
            b = 0x0000;
            m = 0x0000;
        }
    }

    rv = (data0 * b) - (data1 * m);
    if (data0*b < data1*m)
        rv = 0;

    rv += 1 << 13;
    return rv >> 14;
}
