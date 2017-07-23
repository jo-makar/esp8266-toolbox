#include <ip_addr.h> /* Must be included before espconn.h */

#include <espconn.h>
#include <osapi.h>

#include "config.h"
#include "httpd.h"
#include "missing-decls.h"

HttpdClient httpd_clients[HTTPD_MAX_CONN];

static struct espconn httpd_conn;
static esp_tcp httpd_tcp;

static void httpd_server_conn_cb(void *arg);
static void httpd_server_error_cb(void *arg, int8_t err);

static void httpd_client_disconn_cb(void *arg);
static void httpd_client_recv_cb(void *arg, char *data, unsigned short len);

static os_event_t httpd_task_queue[HTTPD_MAX_CONN];
enum httpd_task_signal { HTTPD_DISCONN };
static void httpd_task(os_event_t *evt);

ICACHE_FLASH_ATTR void httpd_init() {
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
        HTTPD_CRITICAL("espconn_regist_connectcb() failed")
       
    if (espconn_regist_reconcb(&httpd_conn, httpd_server_error_cb))
        HTTPD_CRITICAL("espconn_regist_reconcb() failed")

    if (espconn_accept(&httpd_conn))
        HTTPD_CRITICAL("espconn_accept() failed")

    /* TODO Are the defaults for espconn_regist_time(), espconn_set_opt()
            and espconn_set_keepalive() acceptable? */

    if (espconn_tcp_set_max_con_allow(&httpd_conn, HTTPD_MAX_CONN))
        HTTPD_CRITICAL("espconn_tcp_set_max_con_allow() failed\n")

    if (!system_os_task(httpd_task, HTTPD_TASK_PRIO, httpd_task_queue,
                        sizeof(httpd_task_queue)/sizeof(*httpd_task_queue)))
        HTTPD_CRITICAL("system_os_task() failed\n")
}

ICACHE_FLASH_ATTR static void httpd_server_conn_cb(void *arg) {
    struct espconn *conn = arg;
    size_t i;

    HTTPD_DEBUG("connect: " IPSTR ":%u\n",
                IP2STR(conn->proto.tcp->remote_ip),
                conn->proto.tcp->remote_port)

    /* Indicate this connection has not been fully initialized */
    conn->reverse = NULL;

    if (espconn_regist_disconcb(conn, httpd_client_disconn_cb)) {
        HTTPD_ERROR("connect: espconn_regist_disconcb() failed\n")
        if (!system_os_post(HTTPD_TASK_PRIO, HTTPD_DISCONN, (uint32_t)conn))
            HTTPD_ERROR("connect: system_os_post() failed\n")
        return;
    }

    if (espconn_regist_recvcb(conn, httpd_client_recv_cb)) {
        HTTPD_ERROR("connect: espconn_regist_recvcb() failed\n")
        if (!system_os_post(HTTPD_TASK_PRIO, HTTPD_DISCONN, (uint32_t)conn))
            HTTPD_ERROR("connect: system_os_post() failed\n")
        return;
    }

    for (i=0; i<sizeof(httpd_clients)/sizeof(*httpd_clients); i++)
        if (!httpd_clients[i].inuse)
            break;
    if (i == sizeof(httpd_clients)/sizeof(*httpd_clients)) {
        HTTPD_WARNING("connect: too many clients\n")
        if (!system_os_post(HTTPD_TASK_PRIO, HTTPD_DISCONN, (uint32_t)conn))
            HTTPD_ERROR("connect: system_os_post() failed\n")
        return;
    }

    os_bzero(httpd_clients + i, sizeof(*httpd_clients));
    httpd_clients[i].inuse = 1;
    httpd_clients[i].conn = conn;

    conn->reverse = &httpd_clients[i];
}

ICACHE_FLASH_ATTR static void httpd_server_error_cb(void *arg, int8_t err) {
    struct espconn *conn = arg;

    #if HTTPD_LOG_LEVEL <= LEVEL_ERROR
    {
        uint32_t t = system_get_time();

        os_printf("%u.%03u: error: %s:%d: ",
                  t/1000000, (t%1000000)/1000, __FILE__, __LINE__);

        switch (err) {
            case ESPCONN_TIMEOUT:
                os_printf("error: timeout ");
                break;
            case ESPCONN_ABRT:
                os_printf("error: abrt ");
                break;
            case ESPCONN_RST:
                os_printf("error: rst ");
                break;
            case ESPCONN_CLSD:
                os_printf("error: clsd ");
                break;
            case ESPCONN_CONN:
                os_printf("error: conn ");
                break;
            case ESPCONN_HANDSHAKE:
                os_printf("error: handshake ");
                break;
            default:
                os_printf("error: unknown (%02x) ");
                break;
        }

        os_printf(IPSTR ":%u\n", IP2STR(conn->proto.tcp->remote_ip),
                conn->proto.tcp->remote_port);
    }
    #endif

    /*
     * TODO How should this be handled?  Likely dependent on the error.
     *      Can verify this is the server socket by checking conn->state.
     *      Unsure if a espconn_disconnect() is needed or just espconn_delete().
     */
}

ICACHE_FLASH_ATTR static void httpd_client_disconn_cb(void *arg) {
    struct espconn *conn = arg;
    HttpdClient *client;

    HTTPD_DEBUG("disconnect: " IPSTR ":%u\n",
                IP2STR(conn->proto.tcp->remote_ip),
                conn->proto.tcp->remote_port)

    if ((client = conn->reverse) != NULL)
        client->inuse = 0;
}

ICACHE_FLASH_ATTR static void httpd_client_recv_cb(void *arg, char *data,
                                                   unsigned short len) {
    struct espconn *conn = arg;
    HttpdClient *client;

    HTTPD_DEBUG("recv: " IPSTR ":%u len=%u\n",
                IP2STR(conn->proto.tcp->remote_ip),
                conn->proto.tcp->remote_port, len)

    /* Should never happen */
    if ((client = conn->reverse) == NULL) {
        HTTPD_ERROR("recv: client not initialized\n")
        if (!system_os_post(HTTPD_TASK_PRIO, HTTPD_DISCONN, (uint32_t)conn))
            HTTPD_ERROR("recv: system_os_post() failed\n")
        return;
    }

    if (client->bufused + len > sizeof(client->buf)) {
        HTTPD_WARNING("recv: client buffer overflow\n")
        if (!system_os_post(HTTPD_TASK_PRIO, HTTPD_DISCONN, (uint32_t)conn))
            HTTPD_ERROR("recv: system_os_post() failed\n")
        return;
    }

    os_memcpy(client->buf + client->bufused, data, len);
    client->bufused += len;

    if (httpd_process(client)) {
        if (!system_os_post(HTTPD_TASK_PRIO, HTTPD_DISCONN, (uint32_t)conn))
            HTTPD_ERROR("recv: system_os_post() failed\n")
    }
}

ICACHE_FLASH_ATTR static void httpd_task(os_event_t *evt) {
    struct espconn *conn;

    switch (evt->sig) {
        case HTTPD_DISCONN: {
            conn = (struct espconn *)evt->par;

            HTTPD_DEBUG("task: disconnect " IPSTR ":%u\n",
                        IP2STR(conn->proto.tcp->remote_ip),
                        conn->proto.tcp->remote_port)

            if (espconn_disconnect(conn))
                HTTPD_ERROR("task: espconn_disconnect() failed\n")
            if (espconn_delete(conn))
                HTTPD_ERROR("task: espconn_delete() failed\n")

            break;
        }

        default:
            HTTPD_ERROR("task: unknown signal (%04x)\n", evt->sig)
            break;
    }
}
