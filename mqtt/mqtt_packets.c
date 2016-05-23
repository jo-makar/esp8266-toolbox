#include "esp-missing-decls.h"
#include "mqtt_init.h"

#include <c_types.h>
#include <osapi.h>
#include <user_interface.h>

#include <stdint.h>

#define MQTT_PKT_INVALID  1
#define MQTT_PKT_OVERFLOW 2

ICACHE_FLASH_ATTR int mqtt_pkt_connect(uint8_t *buf, uint16_t len, uint16_t *used) {
    /*
     * NB The following does not handle passwords with embedded null bytes,
     *    which is possible since passwords are binary data (unlike usernames)
     */

    #define CLIENTID_PREFIX "esp8266_"
    uint8_t clientid[os_strlen(CLIENTID_PREFIX) + 8 + 1];
    /* NB Client id length must be less than or equal to 23 chars */

    uint8_t *pkt = buf;
    
    uint16_t remlen;    /* Remaining length */

    os_snprintf((char *)clientid, sizeof(clientid),
                "%s%08x", CLIENTID_PREFIX, system_get_chip_id());

    remlen = 10 + 2 + (sizeof(clientid)-1);

    #if defined(MQTT_USER) && defined(MQTT_PASS)
        remlen += 2 + os_strlen(MQTT_USER) + 2 + os_strlen(MQTT_PASS);
    #endif

    if (remlen > 255)          return MQTT_PKT_INVALID;
    else if (2 + remlen > len) return MQTT_PKT_OVERFLOW;

    /*
     * Fixed header
     */

    pkt += os_sprintf((char *)pkt, "\x10%c", remlen);

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

    *used = 2 + remlen;
    return 0;
}

ICACHE_FLASH_ATTR int mqtt_pkt_connectack(uint8_t *buf, uint8_t len) {
    uint8_t *pkt = buf;

    uint8_t type;       /* Control packet type */
    uint8_t remlen;     /* Remaining length */
    uint8_t session;    /* Session present flag */
    uint8_t connect;    /* Connect return code */

    if (len < 2)
        return -1;

    /*
     * Fixed header
     */

    type   = (*(pkt++) >> 4) & 0x0f;
    remlen = *(pkt++);

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

    uint8_t type;       /* Control packet type */
    uint8_t remlen;     /* Remaining length */

    if (len < 2)
        return -1;

    /*
     * Fixed header
     */

    type   = (*(pkt++) >> 4) & 0x0f;
    remlen =  *(pkt++);

    if (type != 0xd)
        return -1;
    if (remlen != 0)
        return -1;

    return 0;
}

ICACHE_FLASH_ATTR int mqtt_pkt_publish(uint8_t *buf, uint16_t len, uint16_t *used, uint8_t *topic, uint8_t *data) {
    uint8_t *pkt = buf;

    uint16_t remlen;    /* Remaining length */

    if (os_strlen((char *)topic) == 0)
        return MQTT_PKT_INVALID;

    remlen = 2 + os_strlen((char *)topic) + os_strlen((char *)data);

    if (remlen > 255)          return MQTT_PKT_INVALID;
    else if (2 + remlen > len) return MQTT_PKT_OVERFLOW;

    /*
     * Fixed header
     */

    /*
     *  DUP flag = 0
     * QoS level = 0
     *    RETAIN = 0
     */
   pkt += os_sprintf((char *)pkt, "\x30%c", remlen);

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

   *used = 2 + remlen;
   return 0;
}
