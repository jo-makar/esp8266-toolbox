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

static void httpd_server_conn_cb(void *arg);
static void httpd_server_error_cb(void *arg, int8_t err);

static void httpd_client_disconn_cb(void *arg);
static void httpd_client_recv_cb(void *arg, char *data, unsigned short len);

static os_event_t httpd_task_queue[MAX_CONN_INBOUND];
enum httpd_task_signal { HTTPD_DISCONN };
static void httpd_task(os_event_t *evt);

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

    if (espconn_regist_connectcb(&httpd_conn, httpd_server_conn_cb))
        FAIL("espconn_regist_connectcb() failed")
       
    if (espconn_regist_reconcb(&httpd_conn, httpd_server_error_cb))
        FAIL("espconn_regist_reconcb() failed")

    if (espconn_accept(&httpd_conn))
        FAIL("espconn_accept() failed")

    /* TODO Are the defaults for espconn_regist_time(), espconn_set_opt()
            and espconn_set_keepalive() acceptable? */

    if (espconn_tcp_set_max_con_allow(&httpd_conn, MAX_CONN_INBOUND))
        FAIL("espconn_tcp_set_max_con_allow() failed\n")

    if (!system_os_task(httpd_task, HTTPD_TASK_PRIO, httpd_task_queue,
                        sizeof(httpd_task_queue)/sizeof(*httpd_task_queue)))
        FAIL("system_os_task() failed\n")
}

static void httpd_server_conn_cb(void *arg) {
    struct espconn *conn = arg;
    size_t i;

    os_printf("httpd: connect: " IPSTR ":%u\n",
              IP2STR(conn->proto.tcp->remote_ip), conn->proto.tcp->remote_port);

    /* Indicate this connection has not been fully initialized */
    conn->reverse = NULL;

    if (espconn_regist_disconcb(conn, httpd_client_disconn_cb)) {
        os_printf("httpd: connect: espconn_regist_disconcb() failed\n");
        system_os_post(HTTPD_TASK_PRIO, HTTPD_DISCONN, (uint32_t)conn);
        return;
    }

    if (espconn_regist_recvcb(conn, httpd_client_recv_cb)) {
        os_printf("httpd: connect: espconn_regist_recvcb() failed\n");
        system_os_post(HTTPD_TASK_PRIO, HTTPD_DISCONN, (uint32_t)conn);
        return;
    }

    for (i=0; i<sizeof(httpd_clients)/sizeof(*httpd_clients); i++)
        if (!httpd_clients[i].inuse)
            break;
    if (i == sizeof(httpd_clients)/sizeof(*httpd_clients)) {
        os_printf("httpd: connect: too many clients\n");
        system_os_post(HTTPD_TASK_PRIO, HTTPD_DISCONN, (uint32_t)conn);
        return;
    }

    httpd_clients[i].inuse = 1;
    httpd_clients[i].conn = conn;
    conn->reverse = &httpd_clients[i];
}

static void httpd_server_error_cb(void *arg, int8_t err) {
    struct espconn *conn = arg;

    switch (err) {
        case ESPCONN_TIMEOUT:
            os_printf("httpd: error: timeout ");
            break;
        case ESPCONN_ABRT:
            os_printf("httpd: error: abrt ");
            break;
        case ESPCONN_RST:
            os_printf("httpd: error: rst ");
            break;
        case ESPCONN_CLSD:
            os_printf("httpd: error: clsd ");
            break;
        case ESPCONN_CONN:
            os_printf("httpd: error: conn ");
            break;
        case ESPCONN_HANDSHAKE:
            os_printf("httpd: error: handshake ");
            break;
        default:
            os_printf("httpd: error: unknown (%02x) ");
            break;
    }

    os_printf(IPSTR ":%u\n", IP2STR(conn->proto.tcp->remote_ip),
              conn->proto.tcp->remote_port);

    /*
     * TODO How should this be handled?  Likely dependent on the error.
     *      Can verify this is the server socket by checking conn->state.
     *      Unsure if a espconn_disconnect() is needed or just espconn_delete().
     */
}

static void httpd_client_disconn_cb(void *arg) {
    struct espconn *conn = arg;
    HttpdClient *client;

    os_printf("httpd: disconnect: " IPSTR ":%u\n",
              IP2STR(conn->proto.tcp->remote_ip), conn->proto.tcp->remote_port);

    if ((client = conn->reverse) != NULL)
        client->inuse = 0;
}

static void httpd_client_recv_cb(void *arg, char *data, unsigned short len) {
    struct espconn *conn = arg;
    HttpdClient *client;

    os_printf("httpd: recv: " IPSTR ":%u len=%u\n",
              IP2STR(conn->proto.tcp->remote_ip),
              conn->proto.tcp->remote_port, len);

    /* Should never happen */
    if ((client = conn->reverse) == NULL) {
        os_printf("httpd: recv: client not initialized\n");
        system_os_post(HTTPD_TASK_PRIO, HTTPD_DISCONN, (uint32_t)conn);
        return;
    }

    if (client->bufused + len > sizeof(client->buf)) {
        os_printf("httpd: recv: client buffer overflow\n");
        system_os_post(HTTPD_TASK_PRIO, HTTPD_DISCONN, (uint32_t)conn);
        return;
    }

    os_memcpy(client->buf + client->bufused, data, len);
    client->bufused += len;

    /* FIXME STOPPED Attempt to process */
}

static void httpd_task(os_event_t *evt) {
    struct espconn *conn;

    switch (evt->sig) {
        case HTTPD_DISCONN: {
            conn = (struct espconn *)evt->par;

            os_printf("httpd: task: disconnect " IPSTR ":%u\n",
                      IP2STR(conn->proto.tcp->remote_ip),
                      conn->proto.tcp->remote_port);

            if (espconn_disconnect(conn))
                os_printf("httpd: task: espconn_disconnect() failed\n");
            if (espconn_delete(conn))
                os_printf("httpd: task: espconn_delete() failed\n");

            break;
        }

        default:
            os_printf("httpd: task: unknown signal (%04x)\n", evt->sig);
            break;
    }
}
