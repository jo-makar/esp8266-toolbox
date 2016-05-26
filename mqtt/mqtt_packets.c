#include "esp-missing-decls.h"
#include "mqtt_init.h"

#include <c_types.h>
#include <osapi.h>
#include <user_interface.h>

#include <stdint.h>

#define MQTT_PKT_INVALID  1
#define MQTT_PKT_OVERFLOW 2

ICACHE_FLASH_ATTR int encode_remlen(uint8_t *buf, uint32_t remlen) {
    /*
     * Encode and write remaining length.
     * Return bytes used or -1 for error (remlen too large).
     */

    if (remlen <= 127)
        return os_sprintf((char *)buf, "%c", remlen);

    else if (remlen <= 127 + 127*128)
        return os_sprintf((char *)buf, "%c%c", 0x80 | (remlen       & 0x7f),
                                                      (remlen >> 7) & 0x7f);

    else if (remlen <= 127 + 127*128 + 127*128*128)
        return os_sprintf((char *)buf, "%c%c%c", 0x80 |  (remlen         & 0x7f),
                                                 0x80 | ((remlen >> 7)   & 0x7f),
                                                         (remlen >> 7*2) & 0x7f);

    else if (remlen <= 127 + 127*128 + 127*128*128 + 127*128*128*128)
        return os_sprintf((char *)buf, "%c%c%c%c", 0x80 |  (remlen         & 0x7f),
                                                   0x80 | ((remlen >> 7)   & 0x7f),
                                                   0x80 | ((remlen >> 7*2) & 0x7f),
                                                           (remlen >> 7*3) & 0x7f);

    else
        return -1;
}

ICACHE_FLASH_ATTR uint32_t decode_remlen(uint8_t *pkt, uint8_t *used) {
    if ((pkt[1] & 0x80) == 0) {
        *used = 1;
        return pkt[1];
    }

    else if ((pkt[2] & 0x80) == 0) {
        *used = 2;
        return (pkt[1] & 0x7f) + pkt[2]*128;
    }

    else if ((pkt[3] & 0x80) == 0) {
        *used = 3;
        return (pkt[1] & 0x7f) + pkt[2]*128 + pkt[3]*128*128;
    }

    else if ((pkt[4] & 0x80) == 0) {
        *used = 4;
        return (pkt[1] & 0x7f) + pkt[2]*128 + pkt[3]*128*128 + pkt[4]*128*128*128;
    }

    else
        /* Two's complement representation is all one bits and exceeds the max possible value */
        return (uint32_t)-1;
}

ICACHE_FLASH_ATTR int mqtt_pkt_connect(uint8_t *buf, uint16_t len, uint16_t *used) {
    /*
     * NB The following does not handle passwords with embedded null bytes,
     *    which is possible since passwords are binary data (unlike usernames)
     */

    /* Only alphanumerics are required to be supported by servers */
    #define CLIENTID_PREFIX "esp"
    uint8_t clientid[os_strlen(CLIENTID_PREFIX) + 8 + 1];
    /* NB Client id length must be less than or equal to 23 chars */

    uint8_t *pkt = buf;
    
    uint16_t remlen;        /* Remaining length */
    int      remlen_used;   /* Remaining length used bytes */

    os_snprintf((char *)clientid, sizeof(clientid),
                "%s%08x", CLIENTID_PREFIX, system_get_chip_id());

    remlen = 10 + 2 + (sizeof(clientid)-1);

    #if defined(MQTT_USER) && defined(MQTT_PASS)
        remlen += 2 + os_strlen(MQTT_USER) + 2 + os_strlen(MQTT_PASS);
    #endif

    if (2 + remlen > len)
        return MQTT_PKT_OVERFLOW;

    /*
     * Fixed header
     */

    pkt += os_sprintf((char *)pkt, "\x10");

    if ((remlen_used = encode_remlen(pkt, remlen)) == -1)
        return MQTT_PKT_INVALID;
    pkt += remlen_used;

    /*
     * Variable header
     */

    pkt += os_sprintf((char *)pkt, "%c\x04MQTT", 0);
    pkt += os_sprintf((char *)pkt, "\x04"); /* MQTT 3.1.1 */

    /*
     *  Clean Session = 1
     *      Will Flag = 0
     *       Will QoS = 0
     *    Will Retain = 0
     *  Password Flag = ?
     * User Name Flag = ?
     */
    {
        uint8_t b = 0x02;

        #if defined(MQTT_USER) && defined(MQTT_PASS)
            b |= 0xc0;
        #endif

        pkt += os_sprintf((char *)pkt, "%c", b);
    }

    pkt += os_sprintf((char *)pkt, "%c%c", (MQTT_KEEPALIVE>>8) & 0x0ff,
                                            MQTT_KEEPALIVE     & 0x0ff);

    /*
     * Payload
     */

    pkt += os_sprintf((char *)pkt, "%c%c", ((sizeof(clientid)-1)>>8) & 0x0ff,
                                            (sizeof(clientid)-1)     & 0x0ff);
    pkt += os_sprintf((char *)pkt, "%s", clientid);

    #if defined(MQTT_USER) && defined(MQTT_PASS)
        pkt += os_sprintf((char *)pkt, "%c%c", (os_strlen(MQTT_USER)>>8) & 0x0ff,
                                                os_strlen(MQTT_USER)     & 0x0ff);
        pkt += os_sprintf((char *)pkt, "%s", MQTT_USER);

        pkt += os_sprintf((char *)pkt, "%c%c", (os_strlen(MQTT_PASS)>>8) & 0x0ff,
                                                os_strlen(MQTT_PASS)     & 0x0ff);
        pkt += os_sprintf((char *)pkt, "%s", MQTT_PASS);
    #endif

    *used = 1 + remlen_used + remlen;
    return 0;
}

ICACHE_FLASH_ATTR int mqtt_pkt_connectack(uint8_t *buf, uint8_t len) {
    uint8_t *pkt = buf;

    uint8_t  type;          /* Control packet type */
    uint32_t remlen;        /* Remaining length */
    uint8_t  remlen_used;   /* Remaining length used bytes */
    uint8_t  session;       /* Session present flag */
    uint8_t  connect;       /* Connect return code */

    if (len < 2)
        return -1;

    /*
     * Fixed header
     */

    type = (*(pkt++) >> 4) & 0x0f;

    if ((remlen = decode_remlen(buf, &remlen_used)) == (uint32_t)-1)
        return -1;
    pkt += remlen_used;

    if (type != 2)
        return -1;
    if (remlen != 2 || len < 4)
        return -1;

    /*
     * Variable header
     */

    session = *(pkt++) & 0x01;
    connect = *(pkt++);

    if (session != 0)
        return -1;

    return connect;
}

ICACHE_FLASH_ATTR int mqtt_pkt_pingreq(uint8_t *buf, uint16_t len, uint16_t *used) {
    uint8_t *pkt = buf;

    if (len < 2)
        return MQTT_PKT_OVERFLOW;

    /*
     * Fixed header
     */

    pkt += os_sprintf((char *)pkt, "\xc0%c", 0);

    *used = 2;
    return 0;
}

ICACHE_FLASH_ATTR int mqtt_pkt_pingresp(uint8_t *buf, uint8_t len) {
    uint8_t *pkt = buf;

    uint8_t type;           /* Control packet type */
    uint32_t remlen;        /* Remaining length */
    uint8_t  remlen_used;   /* Remaining length used bytes */

    if (len < 2)
        return -1;

    /*
     * Fixed header
     */

    type = (*(pkt++) >> 4) & 0x0f;

    if ((remlen = decode_remlen(buf, &remlen_used)) == (uint32_t)-1)
        return -1;
    pkt += remlen_used;

    if (type != 0xd)
        return -1;
    if (remlen != 0)
        return -1;

    return 0;
}

ICACHE_FLASH_ATTR int mqtt_pkt_publish(uint8_t *buf, uint16_t len, uint16_t *used, uint8_t *topic, uint8_t *data) {
    uint8_t *pkt = buf;

    uint16_t remlen;        /* Remaining length */
    int      remlen_used;   /* Remaining length used bytes */

    if (os_strlen((char *)topic) == 0)
        return MQTT_PKT_INVALID;

    remlen = 2 + os_strlen((char *)topic) + os_strlen((char *)data);

    if (2 + remlen > len)
        return MQTT_PKT_OVERFLOW;

    /*
     * Fixed header
     */

    /*
     *  DUP flag = 0
     * QoS level = 0
     *    RETAIN = 0
     */
   pkt += os_sprintf((char *)pkt, "\x30");

   if ((remlen_used = encode_remlen(pkt, remlen)) == -1)
       return MQTT_PKT_INVALID;
   pkt += remlen_used;

   /*
    * Variable header
    */

   pkt += os_sprintf((char *)pkt, "%c%c", (os_strlen((char *)topic)>>8) & 0x0ff,
                                           os_strlen((char *)topic)     & 0x0ff);
   pkt += os_sprintf((char *)pkt, "%s", topic);

   /* Packet identifiers are only for QoS levels 1 and 2 */

   /*
    * Payload
    */

   pkt += os_sprintf((char *)pkt, "%s", data);

   *used = 1 + remlen_used + remlen;
   return 0;
}
