#include "esp-missing-decls.h"
#include "httpd/httpclient.h"
#include "httpd/httpd_process.h"

/* Must be included before espconn.h */
#include <ip_addr.h>

#include <espconn.h>
#include <osapi.h>

HttpClient clients[5];

struct espconn httpdconn;
esp_tcp httpdtcp;

void    httpd_connect_cb(void *arg);
void httpd_disconnect_cb(void *arg);
void      httpd_error_cb(void *arg, int8_t err);
void       httpd_recv_cb(void *arg, char *data, unsigned short len);

void httpd_init() {
    {
        size_t i;
        for (i=0; i<sizeof(clients)/sizeof(*clients); i++)
            os_bzero(clients+i, sizeof(*clients));
    }

    {
        httpdconn.type  = ESPCONN_TCP;
        httpdconn.state = ESPCONN_NONE;

        httpdtcp.local_port = 80;
        /* httpdtcp.local_ip can be left unset */

        httpdconn.proto.tcp = &httpdtcp;

        if (espconn_regist_connectcb(&httpdconn, httpd_connect_cb)) {
            os_printf("httpd_init: espconn_regist_connectcb failed\n");
            return;
        }
        if (espconn_regist_reconcb(&httpdconn, httpd_error_cb)) {
            os_printf("httpd_init: espconn_regist_reconcb failed\n");
            return;
        }

        if (espconn_accept(&httpdconn)) {
            os_printf("httpd_init: espconn_accept failed\n");
            return;
        }
    }
}

void httpd_connect_cb(void *arg) {
    struct espconn *conn = arg;
    size_t i;

    /* Sanity checks */
    if (conn->type != ESPCONN_TCP)
        goto drop;
    if (conn->state != ESPCONN_CONNECT)
        goto drop;

    os_printf("httpd_connect_cb: connect from " IPSTR ":%u\n",
              IP2STR(conn->proto.tcp->remote_ip), conn->proto.tcp->remote_port);

    for (i=0; i<sizeof(clients)/sizeof(*clients); i++)
        if (!clients[i].inuse)
            break;
    if (i == sizeof(clients)/sizeof(*clients)) {
        os_printf("httpd_connect_cb: too many active clients\n");
        goto drop;
    }

    clients[i].inuse = 1;
    os_memcpy(clients[i].ip, conn->proto.tcp->remote_ip, 4);
    clients[i].port = conn->proto.tcp->remote_port;

    if (espconn_regist_disconcb(conn, httpd_disconnect_cb))
        os_printf("httpd_connect_cb: espconn_regist_disconcb failed\n");
    if (espconn_regist_recvcb(conn, httpd_recv_cb))
        os_printf("httpd_connect_cb: espconn_regist_recvcb failed\n");

    /* Not using espconn_regist_sentcb because it does not report the amount of data sent */

    return;

    drop:
        if (espconn_disconnect(conn))
            os_printf("httpd_connect_cb: espconn_disconnect failed\n");
}

#define FIND_CLIENT \
    for (i=0; i<sizeof(clients)/sizeof(*clients); i++) \
        if (clients[i].inuse && \
                os_memcmp(clients[i].ip, conn->proto.tcp->remote_ip, 4) == 0 && \
                clients[i].port == conn->proto.tcp->remote_port) \
            break;

void httpd_disconnect_cb(void *arg) {
    struct espconn *conn = arg;
    size_t i;

    os_printf("httpd_disconnect_cb: disconnect from " IPSTR ":%u\n",
              IP2STR(conn->proto.tcp->remote_ip), conn->proto.tcp->remote_port);

    FIND_CLIENT
    if (i == sizeof(clients)/sizeof(*clients))
        os_printf("httpd_disconnect_cb: disconnect from unknown client\n");
    else {
        /* Implicitly sets attributes in clients[i] to zero (eg inuse, status, ..) */
        os_bzero(clients+i, sizeof(*(clients+i)));
    }
}

void httpd_error_cb(void *arg, int8_t err) {
    struct espconn *conn = arg;
    size_t i;

    os_printf("httpd_error_cb: error %d ", err);
    switch (err) {
        case ESPCONN_OK:            os_printf("ok ");                           break;
        case ESPCONN_MEM:           os_printf("out of memory ");                break;
        case ESPCONN_TIMEOUT:       os_printf("timeout ");                      break;
        case ESPCONN_RTE:           os_printf("routing ");                      break;
        case ESPCONN_INPROGRESS:    os_printf("in progress ");                  break;
        case ESPCONN_MAXNUM:        os_printf("max number ");                   break;
        case ESPCONN_ABRT:          os_printf("connection aborted ");           break;

        case ESPCONN_RST:           os_printf("connection reset ");             break;
        case ESPCONN_CLSD:          os_printf("connection closed ");            break;
        case ESPCONN_CONN:          os_printf("not connected ");                break;

        case ESPCONN_ARG:           os_printf("illegal argument ");             break;
        case ESPCONN_IF:            os_printf("low-level error ");              break;
        case ESPCONN_ISCONN:        os_printf("already connected ");            break;

        case ESPCONN_HANDSHAKE:     os_printf("SSL handshake failed ");         break;
        /* case ESPCONN_RESP_TIMEOUT:  os_printf("SSL handshake no response ");    break; */
        /* case ESPCONN_PROTO_MSG:     os_printf("SSL application invalid ");      break; */

        default:                    os_printf("unknown ");                      break;
    }
    os_printf("from " IPSTR ":%u\n", IP2STR(conn->proto.tcp->remote_ip), conn->proto.tcp->remote_port);

    FIND_CLIENT
    if (i == sizeof(clients)/sizeof(*clients))
        os_printf("httpd_error_cb: error from unknown client\n");

    else {
        if (espconn_disconnect(conn))
            os_printf("httpd_error_cb: espconn_disconnect failed\n");

        /* Implicitly sets attributes in clients[i] to zero (eg inuse, status, ..) */
        os_bzero(clients+i, sizeof(*(clients+i)));
    }
}

void httpd_recv_cb(void *arg, char *data, unsigned short len) {
    struct espconn *conn = arg;
    size_t i;

    os_printf("httpd_recv_cb: received %u bytes from " IPSTR ":%u\n",
              len, IP2STR(conn->proto.tcp->remote_ip), conn->proto.tcp->remote_port);

    FIND_CLIENT
    if (i == sizeof(clients)/sizeof(*clients)) {
        os_printf("httpd_recv_cb: receive from unknown client\n");
        return;
    }

    if ((size_t)(clients[i].bufused + len) > sizeof(clients[i].buf)) {
        os_printf("httpd_recv_cb: insufficient buffer space; available=%u, required=%u\n",
                  sizeof(clients[i].buf) - clients[i].bufused, len);
        if (espconn_disconnect(conn))
            os_printf("httpd_recv_cb: espconn_disconnect failed\n");
    }

    os_memcpy(clients[i].buf + clients[i].bufused, data, len);
    clients[i].bufused += len;

    httpd_process(conn, &clients[i]);
}
