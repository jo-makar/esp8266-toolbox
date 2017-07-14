#include <ip_addr.h> /* Must be included before espconn.h */

#include <espconn.h>
#include <osapi.h>

#include "config.h"
#include "httpd.h"
#include "missing-decls.h"
#include "utils.h"

HttpdClient httpd_clients[MAX_CONN_INBOUND];

static struct espconn httpd_conn;
static esp_tcp httpd_tcp;

static void httpd_conn_cb(void *arg);
static void httpd_error_cb(void *arg, int8_t err);

void httpd_init() {
    size_t i;

    for (i=0; i<sizeof(httpd_clients)/sizeof(*httpd_clients); i++)
        os_bzero(httpd_clients + i, sizeof(*httpd_clients));

    os_bzero(&httpd_conn, sizeof(httpd_conn));
    httpd_conn.type = ESPCONN_TCP;
    httpd_conn.state = ESPCONN_NONE;

    os_bzero(&httpd_tcp, sizeof(esp_tcp));
    httpd_tcp.local_port = 80;

    httpd_conn.proto.tcp = &httpd_tcp;

    if (espconn_regist_connectcb(&httpd_conn, httpd_conn_cb))
        FAIL("espconn_regist_connectcb() failed")
       
    if (espconn_regist_reconcb(&httpd_conn, httpd_error_cb))
        FAIL("espconn_regist_reconcb() failed")

    if (espconn_accept(&httpd_conn))
        FAIL("espconn_accept() failed")

    /* TODO Are the defaults for espconn_regist_time(), espconn_set_opt()
            and espconn_set_keepalive() acceptable? */

    if (espconn_tcp_set_max_con_allow(&httpd_conn, MAX_CONN_INBOUND))
        FAIL("espconn_tcp_set_max_con_allow() failed\n")

    os_printf("espconn_tcp_get_max_con_allow = %d\n", espconn_tcp_get_max_con_allow(&httpd_conn));
}

static void httpd_conn_cb(void *arg) {
    struct espconn *conn = arg;

    os_printf("httpd: connect: " IPSTR ":%u\n",
              IP2STR(conn->proto.tcp->remote_ip), conn->proto.tcp->remote_port);

    /* FIXME STOPPED setup a pointer to conn->reverse
                     search for inuse=0 set it & assign to conn */

    /* espconn_regist_sentcb - generic tcp/udp sent data callback */ 
    /* OR espconn_regist_write_finish - tcp sent data callback */

    /* espconn_regist_recvcb - generic tcp/udp recvd data callback */
    /* espconn_regist_disconcb */
}

static void httpd_error_cb(void *arg, int8_t err) {
    /* FIXME STOPPED */
}
