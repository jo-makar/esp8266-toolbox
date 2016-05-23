#ifndef MQTT_PACKETS_H
#define MQTT_PACKETS_H

#include <stdint.h>

int mqtt_pkt_connect(uint8_t *buf, uint16_t len, uint16_t *used);
int mqtt_pkt_connectack(uint8_t *buf, uint8_t len);

int mqtt_pkt_pingreq(uint8_t *buf, uint16_t len, uint16_t *used);
int mqtt_pkt_pingresp(uint8_t *buf, uint8_t len);

int mqtt_pkt_publish(uint8_t *buf, uint16_t len, uint16_t *used, uint8_t *topic, uint8_t *data);

#endif
