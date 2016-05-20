#include "esp-missing-decls.h"
#include "mqtt_init.h"
#include "mqtt_packets.h"

/* Must be included before espconn.h */
#include <ip_addr.h>

#include <espconn.h>
#include <osapi.h>

ICACHE_FLASH_ATTR void mqtt_process(struct espconn *conn) {
    uint8_t type;       /* Control packet type */
    uint8_t remlen;     /* Remaining length */

    int rv;
    int consume = 0;
    
    if (mqttstate.bufused < 2)
        return;

    /*
     * Fixed header
     */

    type   = (mqttstate.buf[0] >> 4) & 0x0f;
    remlen = mqttstate.buf[1];

    if (mqttstate.bufused - 2 < remlen)
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

    if (consume) {
        os_memmove(mqttstate.buf, mqttstate.buf + 2+remlen, mqttstate.bufused - (2+remlen));
        mqttstate.bufused -= 2 + remlen;
    }

    /* FIXME STOPPED Create an mqtt task (in user_main()) that publishes data and reconnects (also pings?) as necessary */
}
