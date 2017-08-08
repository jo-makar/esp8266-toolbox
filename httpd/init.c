#include <ip_addr.h> /* Must be included before espconn.h */

#include <espconn.h>
#include <osapi.h>

#include "config.h"
#include "httpd.h"
#include "log.h"
#include "missing-decls.h"

HttpdClient httpd_clients[HTTPD_MAX_CONN];

static struct espconn httpd_conn;
static esp_tcp httpd_tcp;

static void httpd_server_conn_cb(void *arg);
static void httpd_server_error_cb(void *arg, int8_t err);

static void httpd_client_disconn_cb(void *arg);
static void httpd_client_recv_cb(void *arg, char *data, unsigned short len);

static os_event_t httpd_task_queue[HTTPD_MAX_CONN];
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

    if (espconn_regist_connectcb(&httpd_conn, httpd_server_conn_cb)) {
        LOG_CRITICAL(HTTPD, "espconn_regist_connectcb() failed\n")
        return;
    }
       
    if (espconn_regist_reconcb(&httpd_conn, httpd_server_error_cb)) {
        LOG_CRITICAL(HTTPD, "espconn_regist_reconcb() failed\n")
        return;
    }

    if (espconn_accept(&httpd_conn)) {
        LOG_CRITICAL(HTTPD, "espconn_accept() failed\n")
        return;
    }

    /* TODO Are the defaults for espconn_regist_time(), espconn_set_opt()
            and espconn_set_keepalive() acceptable? */

    if (espconn_tcp_set_max_con_allow(&httpd_conn, HTTPD_MAX_CONN)) {
        LOG_CRITICAL(HTTPD, "espconn_tcp_set_max_con_allow() failed\n")
        return;
    }

    /*
     * Use a task (rather than a timer) because multiple clients may need to
     * be signalled (ie disconnected) simultaneously or near simultaneously.
     */
    if (!system_os_task(httpd_task, HTTPD_TASK_PRIO, httpd_task_queue,
                        sizeof(httpd_task_queue)/sizeof(*httpd_task_queue))) {
        LOG_CRITICAL(HTTPD, "system_os_task() failed\n")
        return;
    }
}

ICACHE_FLASH_ATTR static void httpd_server_conn_cb(void *arg) {
    struct espconn *conn = arg;
    size_t i;

    LOG_DEBUG(HTTPD, "connect: " IPSTR ":%u\n",
                     IP2STR(conn->proto.tcp->remote_ip),
                     conn->proto.tcp->remote_port)

    /* Indicate this connection has not been fully initialized */
    conn->reverse = NULL;

    if (espconn_regist_disconcb(conn, httpd_client_disconn_cb)) {
        LOG_ERROR(HTTPD, "connect: espconn_regist_disconcb() failed\n")
        if (!system_os_post(HTTPD_TASK_PRIO, HTTPD_DISCONN, (uint32_t)conn))
            LOG_ERROR(HTTPD, "connect: system_os_post() failed\n")
        return;
    }

    if (espconn_regist_recvcb(conn, httpd_client_recv_cb)) {
        LOG_ERROR(HTTPD, "connect: espconn_regist_recvcb() failed\n")
        if (!system_os_post(HTTPD_TASK_PRIO, HTTPD_DISCONN, (uint32_t)conn))
            LOG_ERROR(HTTPD, "connect: system_os_post() failed\n")
        return;
    }

    for (i=0; i<sizeof(httpd_clients)/sizeof(*httpd_clients); i++)
        if (!httpd_clients[i].inuse)
            break;
    if (i == sizeof(httpd_clients)/sizeof(*httpd_clients)) {
        LOG_WARNING(HTTPD, "connect: too many clients\n")
        if (!system_os_post(HTTPD_TASK_PRIO, HTTPD_DISCONN, (uint32_t)conn))
            LOG_ERROR(HTTPD, "connect: system_os_post() failed\n")
        return;
    }

    os_bzero(httpd_clients + i, sizeof(*httpd_clients));
    httpd_clients[i].inuse = 1;
    httpd_clients[i].conn = conn;

    conn->reverse = &httpd_clients[i];
}

ICACHE_FLASH_ATTR static void httpd_server_error_cb(void *arg, int8_t err) {
    struct espconn *conn = arg;

    switch (err) {
        case ESPCONN_TIMEOUT:
            LOG_ERROR(HTTPD, "error: timeout " IPSTR ":%u\n",
                             IP2STR(conn->proto.tcp->remote_ip),
                             conn->proto.tcp->remote_port)
            break;
        case ESPCONN_ABRT:
            LOG_ERROR(HTTPD, "error: abrt " IPSTR ":%u\n",
                             IP2STR(conn->proto.tcp->remote_ip),
                             conn->proto.tcp->remote_port)
            break;
        case ESPCONN_RST:
            LOG_ERROR(HTTPD, "error: rst " IPSTR ":%u\n",
                             IP2STR(conn->proto.tcp->remote_ip),
                             conn->proto.tcp->remote_port)
            break;
        case ESPCONN_CLSD:
            LOG_ERROR(HTTPD, "error: clsd " IPSTR ":%u\n",
                             IP2STR(conn->proto.tcp->remote_ip),
                             conn->proto.tcp->remote_port)
            break;
        case ESPCONN_CONN:
            LOG_ERROR(HTTPD, "error: conn " IPSTR ":%u\n",
                             IP2STR(conn->proto.tcp->remote_ip),
                             conn->proto.tcp->remote_port)
            break;
        case ESPCONN_HANDSHAKE:
            LOG_ERROR(HTTPD, "error: handshake " IPSTR ":%u\n",
                             IP2STR(conn->proto.tcp->remote_ip),
                             conn->proto.tcp->remote_port)
            break;
        default:
            LOG_ERROR(HTTPD, "error: unknown (%02x) " IPSTR ":%u\n",
                             err, IP2STR(conn->proto.tcp->remote_ip),
                             conn->proto.tcp->remote_port)
            break;
    }

    /*
     * TODO How should this be handled?  Likely dependent on the error.
     *      Can verify this is the server socket by checking conn->state.
     *      Unsure if a espconn_disconnect() is needed or just espconn_delete().
     */
}

ICACHE_FLASH_ATTR static void httpd_client_disconn_cb(void *arg) {
    struct espconn *conn = arg;
    HttpdClient *client;

    LOG_DEBUG(HTTPD, "disconnect: " IPSTR ":%u\n",
                     IP2STR(conn->proto.tcp->remote_ip),
                     conn->proto.tcp->remote_port)

    if ((client = conn->reverse) != NULL)
        client->inuse = 0;
}

ICACHE_FLASH_ATTR static void httpd_client_recv_cb(void *arg, char *data,
                                                   unsigned short len) {
    struct espconn *conn = arg;
    HttpdClient *client;

    LOG_DEBUG(HTTPD, "recv: " IPSTR ":%u len=%u\n",
                     IP2STR(conn->proto.tcp->remote_ip),
                     conn->proto.tcp->remote_port, len)

    /* Should never happen */
    if ((client = conn->reverse) == NULL) {
        LOG_ERROR(HTTPD, "recv: client not initialized\n")
        if (!system_os_post(HTTPD_TASK_PRIO, HTTPD_DISCONN, (uint32_t)conn))
            LOG_ERROR(HTTPD, "recv: system_os_post() failed\n")
        return;
    }

    if (client->bufused + len > sizeof(client->buf)) {
        LOG_WARNING(HTTPD, "recv: client buffer overflow\n")
        if (!system_os_post(HTTPD_TASK_PRIO, HTTPD_DISCONN, (uint32_t)conn))
            LOG_ERROR(HTTPD, "recv: system_os_post() failed\n")
        return;
    }

    os_memcpy(client->buf + client->bufused, data, len);
    client->bufused += len;

    if (httpd_process(client)) {
        if (!system_os_post(HTTPD_TASK_PRIO, HTTPD_DISCONN, (uint32_t)conn))
            LOG_ERROR(HTTPD, "recv: system_os_post() failed\n")
    }
}

ICACHE_FLASH_ATTR static void httpd_task(os_event_t *evt) {
    struct espconn *conn;

    switch (evt->sig) {
        case HTTPD_DISCONN: {
            conn = (struct espconn *)evt->par;

            LOG_DEBUG(HTTPD, "task: disconnect " IPSTR ":%u\n",
                             IP2STR(conn->proto.tcp->remote_ip),
                             conn->proto.tcp->remote_port)

            if (espconn_disconnect(conn))
                LOG_ERROR(HTTPD, "task: espconn_disconnect() failed\n")
            if (espconn_delete(conn))
                LOG_ERROR(HTTPD, "task: espconn_delete() failed\n")

            break;
        }

        default:
            LOG_ERROR(HTTPD, "task: unknown signal (%04x)\n", evt->sig)
            break;
    }
}
