#ifndef MQTT_PACKETS_H
#define MQTT_PACKETS_H

#include <stdint.h>

int mqtt_pkt_connect(uint8_t *buf, uint16_t len, uint16_t *used);
int mqtt_pkt_connectack(uint8_t *buf, uint8_t len);

#endif
