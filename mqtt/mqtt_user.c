#include "esp-missing-decls.h"
#include "mqtt_init.h"
#include "mqtt_packets.h"

#include <c_types.h>
#include <osapi.h>

ICACHE_FLASH_ATTR void mqtt_user(void *arg) {
    (void)arg;

    uint8_t  buf[512];
    uint16_t used;

    if (mqttstate.connected) {
        /****************************************************************************************/

        if ((uint32_t)(system_get_time() - mqttstate.prevpublish) > MQTT_PUBLISH_RATE*1000000) {
            uint8_t data[32];

            if ((size_t)os_snprintf((char *)data, sizeof(data),
                                    "mqtt_user %u", system_get_time() / 1000000) >= sizeof(data))
                data[sizeof(data)-1] = 0;

            os_printf("mqtt_user: publish \"%s\"\n", data);

            if (mqtt_pkt_publish(buf, sizeof(buf), &used, (uint8_t *)MQTT_PUBLISH_TOPIC, data)) {
                os_printf("mqtt_user: mqtt_pkt_publish failed\n");
                return;
            }

            if (espconn_send(&mqttconn, buf, used)) {
                os_printf("mqtt_user: espconn_send failed\n");
                return;
            }

            mqttstate.prevpublish = mqttstate.prevactivity = system_get_time();
        }

        /****************************************************************************************/

        /* Send a ping request if half the keep alive time has passed since last activity */
        else if ((uint32_t)(system_get_time() - mqttstate.prevactivity) > MQTT_KEEPALIVE/2*1000000) {
            os_printf("mqtt_user: ping req\n");

            if (mqtt_pkt_pingreq(buf, sizeof(buf), &used)) {
                os_printf("mqtt_user: mqtt_pkt_pingreq failed\n");
                return;
            }

            if (espconn_send(&mqttconn, buf, used)) {
                os_printf("mqtt_user: espconn_send failed\n");
                return;
            }

            mqttstate.prevactivity = system_get_time();
        }
    }

    else {
        /* Attempt to reconnect some time after the last activity (connect attempt or transmit) */
        if ((uint32_t)(system_get_time() - mqttstate.prevactivity) > MQTT_KEEPALIVE*3*1000000)
            /* Attempt to reconnect if the underlying socket is not active */
            if (mqttconn.state == ESPCONN_NONE || mqttconn.state == ESPCONN_CLOSE) {
                os_printf("mqtt_user: reconnect attempt\n");
                mqtt_connect();
            }
    }
}
