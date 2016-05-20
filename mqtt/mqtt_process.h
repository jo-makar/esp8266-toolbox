#ifndef MQTT_PROCESS
#define MQTT_PROCESS

/* Must be included before espconn.h */
#include <ip_addr.h>

#include <espconn.h>

void mqtt_process(struct espconn *conn);

#endif
