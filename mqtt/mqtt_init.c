#include "esp-missing-decls.h"
#include "mqtt_init.h"
#include "mqtt_packets.h"
#include "mqtt_process.h"
#include "mqtt_user.h"

/* Must be included before espconn.h */
#include <ip_addr.h>

#include <espconn.h>
#include <osapi.h>

MqttState mqttstate;

ICACHE_RODATA_ATTR const uint8_t MQTT_HOST[] = {10, 0, 0, 9};

struct espconn mqttconn;
esp_tcp mqtttcp;

void    mqtt_connect_cb(void *arg);
void mqtt_disconnect_cb(void *arg);
void      mqtt_error_cb(void *arg, int8_t err);
void       mqtt_recv_cb(void *arg, char *data, unsigned short len);

volatile os_timer_t mqtt_user_timer;

ICACHE_FLASH_ATTR void mqtt_init() {
    /* Implicitly sets attributes in mqttstate to zero (eg connected, ..) */
    os_bzero(&mqttstate, sizeof(mqttstate));

    {
        mqttconn.type  = ESPCONN_TCP;
        mqttconn.state = ESPCONN_NONE;

        os_memcpy(mqtttcp.remote_ip, MQTT_HOST, sizeof(mqtttcp.remote_ip));
        mqtttcp.remote_port = 1883;
        /* mqtttcp.local_{port,ip} can be left unset */

        mqttconn.proto.tcp = &mqtttcp;

        if (espconn_regist_connectcb(&mqttconn, mqtt_connect_cb)) {
            os_printf("mqtt_init: espconn_regist_connectcb failed\n");
            return;
        }
        if (espconn_regist_reconcb(&mqttconn, mqtt_error_cb)) {
            os_printf("mqtt_init: espconn_regist_reconcb failed\n");
            return;
        }
    }

    os_timer_disarm((os_timer_t *)&mqtt_user_timer);
     os_timer_setfn((os_timer_t *)&mqtt_user_timer, mqtt_user, NULL);
       os_timer_arm((os_timer_t *)&mqtt_user_timer, (MQTT_KEEPALIVE * 1000) / 3, 1);
}

ICACHE_FLASH_ATTR void mqtt_connect() {
    mqttstate.prevactivity = system_get_time();

    if (espconn_connect(&mqttconn))
        os_printf("mqtt_connect: espconn_connect failed\n");
}

ICACHE_FLASH_ATTR void mqtt_connect_cb(void *arg) {
    struct espconn *conn = arg;

    /* Sanity checks */
    if (conn->type != ESPCONN_TCP)
        goto drop;
    if (conn->state != ESPCONN_CONNECT)
        goto drop;

    os_printf("mqtt_connect_cb: connect to " IPSTR ":%u\n",
              IP2STR(conn->proto.tcp->remote_ip), conn->proto.tcp->remote_port);

    if (espconn_regist_disconcb(conn, mqtt_disconnect_cb)) {
        os_printf("mqtt_connect_cb: espconn_regist_disconcb failed\n");
        goto drop;
    }

    if (espconn_regist_recvcb(conn, mqtt_recv_cb)) {
        os_printf("mqtt_connect_cb: espconn_regist_recvcb failed\n");
        goto drop;
    }

    /* Not using espconn_regist_sentcb because it does not report the amount of data sent */

    /* Send a connect packet */
    {
        uint8_t  buf[512];
        uint16_t used;

        if (mqtt_pkt_connect(buf, sizeof(buf), &used)) {
            os_printf("mqtt_connect_cb: mqtt_pkt_connect failed\n");
            goto drop;
        }

        if (espconn_send(&mqttconn, buf, used)) {
            os_printf("mqtt_connect_cb: espconn_send failed\n");
            goto drop;
        }
    }

    return;

    drop:
        if (espconn_disconnect(conn))
            os_printf("mqtt_connect_cb: espconn_disconnect failed\n");
}

ICACHE_FLASH_ATTR void mqtt_disconnect_cb(void *arg) {
    struct espconn *conn = arg;

    os_printf("mqtt_disconnect_cb: disconnect from " IPSTR ":%u\n",
              IP2STR(conn->proto.tcp->remote_ip), conn->proto.tcp->remote_port);

    mqttstate.connected = 0;
    mqttstate.bufused   = 0;
}

ICACHE_FLASH_ATTR void mqtt_error_cb(void *arg, int8_t err) {
    struct espconn *conn = arg;

    os_printf("mqtt_error_cb: error %d ", err);
    switch (err) {
        case ESPCONN_OK:             os_printf("ok ");                           break;
        case ESPCONN_MEM:            os_printf("out of memory ");                break;
        case ESPCONN_TIMEOUT:        os_printf("timeout ");                      break;
        case ESPCONN_RTE:            os_printf("routing ");                      break;
        case ESPCONN_INPROGRESS:     os_printf("in progress ");                  break;
        case ESPCONN_MAXNUM:         os_printf("max number ");                   break;
        case ESPCONN_ABRT:           os_printf("connection aborted ");           break;

        case ESPCONN_RST:            os_printf("connection reset ");             break;
        case ESPCONN_CLSD:           os_printf("connection closed ");            break;
        case ESPCONN_CONN:           os_printf("not connected ");                break;

        case ESPCONN_ARG:            os_printf("illegal argument ");             break;
        case ESPCONN_IF:             os_printf("low-level error ");              break;
        case ESPCONN_ISCONN:         os_printf("already connected ");            break;

        case ESPCONN_HANDSHAKE:      os_printf("SSL handshake failed ");         break;
        /*case ESPCONN_RESP_TIMEOUT: os_printf("SSL handshake no response ");    break;*/
        /*case ESPCONN_PROTO_MSG:    os_printf("SSL application invalid ");      break;*/

        default:                     os_printf("unknown ");                      break;
    }
    os_printf("from " IPSTR ":%u\n", IP2STR(conn->proto.tcp->remote_ip), conn->proto.tcp->remote_port);

    if (espconn_disconnect(conn))
        os_printf("mqtt_error_cb: espconn_disconnect failed\n");
}

ICACHE_FLASH_ATTR void mqtt_recv_cb(void *arg, char *data, unsigned short len) {
    struct espconn *conn = arg;

    os_printf("mqtt_recv_cb: received %u bytes from " IPSTR ":%u\n",
              len, IP2STR(conn->proto.tcp->remote_ip), conn->proto.tcp->remote_port);

    if (len > sizeof(mqttstate.buf)) {
        os_printf("mqtt_recv_cb: insufficient buffer length; maximum=%u, required=%u\n",
                  sizeof(mqttstate.buf), len);

        if (espconn_disconnect(conn))
            os_printf("mqtt_recv_cb: espconn_disconnect failed\n");
    }

    else if (len > sizeof(mqttstate.buf) - mqttstate.bufused) {

        os_printf("mqtt_recv_cb: insufficient buffer length; available=%u, required=%u\n",
                  sizeof(mqttstate.buf) - mqttstate.bufused, len);

        if (espconn_disconnect(conn))
            os_printf("mqtt_recv_cb: espconn_disconnect failed\n");
    }

    os_memcpy(mqttstate.buf + mqttstate.bufused, data, len);
    mqttstate.bufused += len;

    mqtt_process(conn);
}
