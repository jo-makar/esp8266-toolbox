#ifndef HTTP_INIT_H
#define HTTP_INIT_H

#include "../log/log.h"
#include "../missing-decls.h"
#include "http.h"

#include <osapi.h>

#define HTTP_MAX_CLIENTS 4
static HttpClient http_clients[HTTP_MAX_CLIENTS];

static struct espconn http_conn;
static esp_tcp http_tcp;

static os_event_t http_task_queue[HTTP_MAX_CLIENTS];
static void http_task(os_event_t *evt);

static void http_server_conn_cb(void *arg);
static void http_server_error_cb(void *arg, int8_t err);

static void http_client_disconn_cb(void *arg);
static void http_client_recv_cb(void *arg, char *data, unsigned short len);

ICACHE_FLASH_ATTR void http_init() {
    os_bzero(http_clients, sizeof(http_clients));

    os_bzero(&http_tcp, sizeof(http_tcp));
    http_tcp.local_port = 80;

    os_bzero(&http_conn, sizeof(http_conn));
    http_conn.type = ESPCONN_TCP;
    http_conn.state = ESPCONN_NONE;
    http_conn.proto.tcp = &http_tcp;

    if (!system_os_task(http_task, HTTP_TASK_PRIO, http_task_queue, HTTP_MAX_CLIENTS))
        CRITICAL(HTTP, "system_os_task() failed")

    if (espconn_regist_connectcb(&http_conn, http_server_conn_cb))
        CRITICAL(HTTP, "espconn_regist_connectcb() failed")

    if (espconn_regist_reconcb(&http_conn, http_server_error_cb))
        CRITICAL(HTTP, "espconn_regist_reconcb() failed")

    if (espconn_accept(&http_conn))
        CRITICAL(HTTP, "espconn_secure_accept() failed")
}

ICACHE_FLASH_ATTR void http_task(os_event_t *evt) {
    struct espconn *conn;

    switch (evt->sig) {
        case HTTP_TASK_KILL: {
            conn = (struct espconn *)evt->par;

            if (espconn_disconnect(conn))
                ERROR(HTTP, "task: espconn_disconnect() failed")
            if (espconn_delete(conn))
                ERROR(HTTP, "task: espconn_delete() failed")

            break;
        }

        default:
            ERROR(HTTP, "task: unknown signal (%04x)", evt->sig)
            break;
    }
}

ICACHE_FLASH_ATTR static void http_server_conn_cb(void *arg) {
    struct espconn *conn = arg;
    size_t i;

    DEBUG(HTTP, "connect: " IPSTR ":%u",
                IP2STR(conn->proto.tcp->remote_ip),
                conn->proto.tcp->remote_port)

    conn->reverse = NULL;

    if (espconn_regist_disconcb(conn, http_client_disconn_cb)) {
        ERROR(HTTP, "connect: espconn_regist_disconcb() failed")
        if (!system_os_post(HTTP_TASK_PRIO, HTTP_TASK_KILL, (uint32_t)conn))
            ERROR(HTTP, "connect: system_os_post() failed")
        return;
    }

    if (espconn_regist_recvcb(conn, http_client_recv_cb)) {
        ERROR(HTTP, "connect: espconn_regist_recvcb() failed")
        if (!system_os_post(HTTP_TASK_PRIO, HTTP_TASK_KILL, (uint32_t)conn))
            ERROR(HTTP, "connect: system_os_post() failed");
        return;
    }

    for (i=0; i<sizeof(http_clients)/sizeof(*http_clients); i++)
        if (!http_clients[i].inuse)
            break;
    if (i == sizeof(http_clients)/sizeof(*http_clients)) {
        WARNING(HTTP, "connect: too many clients")
        if (!system_os_post(HTTP_TASK_PRIO, HTTP_TASK_KILL, (uint32_t)conn))
            ERROR(HTTP, "connect: system_os_post() failed")
        return;
    }

    os_bzero(http_clients + i, sizeof(*http_clients));
    http_clients[i].inuse = 1;
    http_clients[i].conn = conn;

    conn->reverse = &http_clients[i];
}

ICACHE_FLASH_ATTR static void http_server_error_cb(void *arg, int8_t err) {
    struct espconn *conn = arg;

    switch (err) {
        case ESPCONN_TIMEOUT:
            ERROR(HTTP, "error: timeout " IPSTR ":%u",
                        IP2STR(conn->proto.tcp->remote_ip),
                        conn->proto.tcp->remote_port)
            break;
        case ESPCONN_ABRT:
            ERROR(HTTP, "error: abrt " IPSTR ":%u",
                         IP2STR(conn->proto.tcp->remote_ip),
                         conn->proto.tcp->remote_port)
            break;
        case ESPCONN_RST:
            ERROR(HTTP, "error: rst " IPSTR ":%u",
                        IP2STR(conn->proto.tcp->remote_ip),
                        conn->proto.tcp->remote_port)
            break;
        case ESPCONN_CLSD:
            ERROR(HTTP, "error: clsd " IPSTR ":%u",
                        IP2STR(conn->proto.tcp->remote_ip),
                        conn->proto.tcp->remote_port)
            break;
        case ESPCONN_CONN:
            ERROR(HTTP, "error: conn " IPSTR ":%u",
                        IP2STR(conn->proto.tcp->remote_ip),
                        conn->proto.tcp->remote_port)
            break;
        case ESPCONN_HANDSHAKE:
            ERROR(HTTP, "error: handshake " IPSTR ":%u",
                        IP2STR(conn->proto.tcp->remote_ip),
                        conn->proto.tcp->remote_port)
            break;
        default:
            ERROR(HTTP, "error: unknown (%02x) " IPSTR ":%u",
                        err, IP2STR(conn->proto.tcp->remote_ip),
                        conn->proto.tcp->remote_port)
            break;
    }
}

ICACHE_FLASH_ATTR static void http_client_disconn_cb(void *arg) {
    struct espconn *conn = arg;
    HttpClient *client;

    DEBUG(HTTP, "disconnect: " IPSTR ":%u",
                 IP2STR(conn->proto.tcp->remote_ip),
                 conn->proto.tcp->remote_port)

    if ((client = conn->reverse) != NULL)
        client->inuse = 0;
}

ICACHE_FLASH_ATTR static void http_client_recv_cb(void *arg, char *data,
                                                   unsigned short len) {
    struct espconn *conn = arg;
    HttpClient *client;

    DEBUG(HTTP, "recv: " IPSTR ":%u len=%u",
                 IP2STR(conn->proto.tcp->remote_ip),
                 conn->proto.tcp->remote_port, len)

    if ((client = conn->reverse) == NULL) {
        ERROR(HTTP, "recv: client not initialized")
        if (!system_os_post(HTTP_TASK_PRIO, HTTP_TASK_KILL, (uint32_t)conn))
            ERROR(HTTP, "recv: system_os_post() failed")
        return;
    }

    if (client->bufused + len > sizeof(client->buf)) {
        WARNING(HTTP, "recv: client buffer overflow")
        if (!system_os_post(HTTP_TASK_PRIO, HTTP_TASK_KILL, (uint32_t)conn))
            ERROR(HTTP, "recv: system_os_post() failed")
        return;
    }

    os_memcpy(client->buf + client->bufused, data, len);
    client->bufused += len;

    if (http_process(client)) {
        if (!system_os_post(HTTP_TASK_PRIO, HTTP_TASK_KILL, (uint32_t)conn))
            ERROR(HTTP, "recv: system_os_post() failed")
    }
}

#endif
