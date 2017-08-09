#include <ip_addr.h> /* Must be included before espconn.h */

#include <espconn.h>
#include <osapi.h>

#include "config.h"
#include "httpd.h"
#include "log.h"
#include "missing-decls.h"

HttpdClient httpd_client;

uint8_t httpd_outbuf[HTTPD_OUTBUF_MAXLEN];
uint16_t httpd_outbuflen;

static struct espconn httpd_conn;
static esp_tcp httpd_tcp;

os_timer_t httpd_disconnect_timer;

static void httpd_server_conn_cb(void *arg);
static void httpd_server_error_cb(void *arg, int8_t err);

static void httpd_client_disconn_cb(void *arg);
static void httpd_client_recv_cb(void *arg, char *data, unsigned short len);

ICACHE_FLASH_ATTR void httpd_init() {
    os_bzero(&httpd_client, sizeof(httpd_client));

    os_bzero(&httpd_tcp, sizeof(esp_tcp));
    httpd_tcp.local_port = 443;

    os_bzero(&httpd_conn, sizeof(httpd_conn));
    httpd_conn.type = ESPCONN_TCP;
    httpd_conn.state = ESPCONN_NONE;
    httpd_conn.proto.tcp = &httpd_tcp;

    if (espconn_regist_connectcb(&httpd_conn, httpd_server_conn_cb)) {
        LOG_CRITICAL(HTTPD, "espconn_regist_connectcb() failed\n")
        return;
    }
       
    if (espconn_regist_reconcb(&httpd_conn, httpd_server_error_cb)) {
        LOG_CRITICAL(HTTPD, "espconn_regist_reconcb() failed\n")
        return;
    }

    if (espconn_secure_accept(&httpd_conn)) {
        LOG_CRITICAL(HTTPD, "espconn_secure_accept() failed\n")
        return;
    }
}

ICACHE_FLASH_ATTR void httpd_stop() {
    /*
     * FIXME
     *
     * If a connection is being processed,
     * mark a flag so that everything is cleaned up when done.
     * Otherwise cleanup now.
     */
}

ICACHE_FLASH_ATTR static void httpd_server_conn_cb(void *arg) {
    struct espconn *conn = arg;

    LOG_DEBUG(HTTPD, "connect: " IPSTR ":%u\n",
                     IP2STR(conn->proto.tcp->remote_ip),
                     conn->proto.tcp->remote_port)

    /* Indicate this connection has not been fully initialized */
    conn->reverse = NULL;

    if (espconn_regist_disconcb(conn, httpd_client_disconn_cb)) {
        LOG_ERROR(HTTPD, "espconn_regist_disconcb() failed\n")
        HTTPD_DISCONNECT_LATER(conn)
        return;
    }

    if (espconn_regist_recvcb(conn, httpd_client_recv_cb)) {
        LOG_ERROR(HTTPD, "espconn_regist_recvcb() failed\n")
        HTTPD_DISCONNECT_LATER(conn)
        return;
    }

    os_bzero(&httpd_client, sizeof(httpd_client));
    httpd_client.conn = conn;

    conn->reverse = &httpd_client;
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

    LOG_DEBUG(HTTPD, "disconnected: " IPSTR ":%u\n",
                     IP2STR(conn->proto.tcp->remote_ip),
                     conn->proto.tcp->remote_port)

    if ((client = conn->reverse) != NULL)
        client->state = HTTPD_STATE_DISCONNECTED;
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
        HTTPD_DISCONNECT_LATER(conn)
        return;
    }

    if (client->bufused + len > sizeof(client->buf)) {
        LOG_WARNING(HTTPD, "recv: client buffer overflow\n")
        HTTPD_DISCONNECT_LATER(conn)
        return;
    }

    os_memcpy(client->buf + client->bufused, data, len);
    client->bufused += len;

    if (httpd_process(client))
        HTTPD_DISCONNECT_LATER(conn)
}

ICACHE_FLASH_ATTR void httpd_disconnect(void *arg) {
    struct espconn *conn = arg;
    HttpdClient *client;

    LOG_DEBUG(HTTPD, "disconnect " IPSTR ":%u\n",
                      IP2STR(conn->proto.tcp->remote_ip),
                      conn->proto.tcp->remote_port)

    if ((client = conn->reverse) != NULL) {
        if (client->state == HTTPD_STATE_DISCONNECTED)
            return;
    }

    if (espconn_secure_disconnect(conn))
        LOG_ERROR(HTTPD, "espconn_secure_disconnect() failed\n")
    if (espconn_secure_delete(conn))
        LOG_ERROR(HTTPD, "espconn_secure_delete() failed\n")
}
