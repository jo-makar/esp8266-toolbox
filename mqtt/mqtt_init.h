#ifndef MQTT_INIT_H
#define MQTT_INIT_H

/* Must be included before espconn.h */
#include <ip_addr.h>

#include <espconn.h>

#include <stddef.h>
#include <stdint.h>

typedef struct {
    #if 0
        #define MQTT_USER "guest"
        #define MQTT_PASS "guest"
    #endif

    #define MQTT_KEEPALIVE 60   /* 0 <= keepalive < 2**16 */

    uint8_t  connected;
    uint32_t prevactivity;      /* Previous connect attempt or transmit time */

    uint8_t buf[1024];
    size_t  bufused;
} MqttState;

extern MqttState mqttstate;

extern struct espconn mqttconn;

void mqtt_init();
void mqtt_connect();

#endif
