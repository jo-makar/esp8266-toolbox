#include "esp-missing-decls.h"
#include "mqtt_init.h"
#include "mqtt_packets.h"

/* Must be included before espconn.h */
#include <ip_addr.h>

#include <espconn.h>
#include <osapi.h>

ICACHE_FLASH_ATTR void mqtt_process(struct espconn *conn) {
    (void)conn;

    uint8_t  type;          /* Control packet type */
    uint32_t remlen;        /* Remaining length */
    uint8_t  remlen_used;   /* Remaining length used bytes */

    int rv;
    int consume = 0;
    
    if (mqttstate.bufused < 2)
        return;

    /*
     * Fixed header
     */

    type = (mqttstate.buf[0] >> 4) & 0x0f;

    /* Invalid remaining length field, mqttstate.buf eventually fills and the connection is reset */
    if ((remlen = decode_remlen(mqttstate.buf, &remlen_used)) == (uint32_t)-1)
        return;

    /* More data anticipated than currently in the buffer, later call will */
    if (mqttstate.bufused - (1 + remlen_used) < remlen)
        return;

    if (type == 2) {
        if ((rv = mqtt_pkt_connectack(mqttstate.buf, mqttstate.bufused)) == -1)
            os_printf("mqtt_process: connect ack: invalid/incomplete packet\n");
        else if (rv == 0) {
            os_printf("mqtt_process: connect ack: connection accepted\n");
            mqttstate.connected = 1;
            consume = 1;
        } else {
            os_printf("mqtt_process: connect ack: connection refused: %u\n", rv);
            consume = 1;
        }
    }

    else if (type == 0xd) {
        if ((rv = mqtt_pkt_pingresp(mqttstate.buf, mqttstate.bufused)) == -1)
            os_printf("mqtt_process: ping resp: invalid/incomplete packet\n");
        else if (rv == 0) {
            os_printf("mqtt_process: ping resp\n");
            consume = 1;
        }
    }

    if (consume) {
        os_memmove(mqttstate.buf,
                   mqttstate.buf     +  1+remlen_used+remlen,
                   mqttstate.bufused - (1+remlen_used+remlen));

        mqttstate.bufused -= 1 + remlen_used + remlen;
    }
}
