#include <osapi.h>

#include "../log.h"
#include "smtp.h"

static Smtp smtp;

static void smtp_send_dns(const char *host, ip_addr_t *ip, void *arg);
static void smtp_send_connect(const ip_addr_t *ip);
static void smtp_send_greet();

static void smtp_conn_cb(void *arg);
static void smtp_error_cb(void *arg, int8_t err);

static void smtp_disconn_cb(void *arg);
static void smtp_recv_cb(void *arg, char *data, unsigned short len);

static void smtp_disconnect();

ICACHE_FLASH_ATTR void smtp_init_gmail(const char *user, const char *pass) {
    size_t len;

    #define GMAIL_HOST "smtp.gmail.com"
    #define GMAIL_PORT 465

    os_bzero(&smtp, sizeof(smtp));
    
    if ((len = os_strlen(GMAIL_HOST) + 1) > sizeof(smtp.host)) {
        LOG_ERROR(SMTP, "smtp host overflow\n")
        return;
    }
    os_strncpy(smtp.host, GMAIL_HOST, len);

    smtp.port = GMAIL_PORT;

    if ((len = os_strlen(user) + 1) > sizeof(smtp.user)) {
        LOG_ERROR(SMTP, "smtp user overflow\n")
        return;
    }
    os_strncpy(smtp.user, user, len);

    if ((len = os_strlen(pass) + 1) > sizeof(smtp.pass)) {
        LOG_ERROR(SMTP, "smtp pass overflow\n")
        return;
    }
    os_strncpy(smtp.pass, pass, len);

    if ((len = os_strlen(user) + 1) > sizeof(smtp.from)) {
        LOG_ERROR(SMTP, "smtp from overflow\n")
        return;
    }
    os_strncpy(smtp.from, user, len);

    smtp.state = SMTP_STATE_READY;
}

ICACHE_FLASH_ATTR void smtp_send(const char *to, const char *subj,
                                 const char *body) {
    #define TIMEOUT 10

    ip_addr_t ip;
    err_t rv;

    if (smtp.state != SMTP_STATE_READY) {
        LOG_ERROR(SMTP, "not in ready state\n")
        return;
    }

    /*
     * Nothing (not event os_delay_us()) "releases" the processor to run tasks,
     * including internal systems task such as handling DNS resolution responses.
     * Or to put it differently no task/timer cannot preempt another task/timer.
     *
     * Hence this process is daisy-chained from a successful DNS resolution.
     */

    smtp.state = SMTP_STATE_RESOLVE;
    rv = espconn_gethostbyname(&smtp.conn, smtp.host, &ip, smtp_send_dns);
    if (rv == ESPCONN_OK) {
        /* No-op taken from cache, smtp.ip populated */
        LOG_DEBUG(SMTP, "gethostbyname() => OK\n")
        smtp_send_connect(&ip);
    } else if (rv == ESPCONN_INPROGRESS) {
        LOG_DEBUG(SMTP, "gethostbyname() => INPROGRESS\n")
    } else {
        LOG_ERROR(SMTP, "espconn_gethosbyname() failed\n");
        smtp.state = SMTP_STATE_FAILED;
    }
}

ICACHE_FLASH_ATTR void smtp_send_dns(const char *host, ip_addr_t *ip,
                                     void *arg) {
    (void)host;
    (void)arg;

    if (ip == NULL) {
        LOG_INFO(SMTP, "smtp dns resolution failed\n");
        smtp.state = SMTP_STATE_FAILED;
    }
    else {
        LOG_INFO(SMTP, "smtp dns host=" IPSTR "\n", IP2STR(ip));
        smtp_send_connect(ip);
    }
}

ICACHE_FLASH_ATTR void smtp_send_connect(const ip_addr_t *ip) {
    smtp.state = SMTP_STATE_CONNECT;

    os_bzero(&smtp.tcp, sizeof(smtp.tcp));
    smtp.tcp.remote_port = smtp.port;
    smtp.tcp.remote_ip[3] = ip->addr >> 24;
    smtp.tcp.remote_ip[2] = ip->addr >> 16;
    smtp.tcp.remote_ip[1] = ip->addr >> 8;
    smtp.tcp.remote_ip[0] = ip->addr;

    os_bzero(&smtp.conn, sizeof(smtp.conn));
    smtp.conn.type = ESPCONN_TCP;
    smtp.conn.proto.tcp = &smtp.tcp;

    if (!espconn_secure_ca_disable(1)) {
        LOG_ERROR(SMTP, "espconn_secure_ca_disable() failed\n")
        smtp.state = SMTP_STATE_FAILED;
        return;
    }

    if (!espconn_secure_set_size(1, 4*1024)) {
        LOG_ERROR(SMTP, "espconn_secure_set_size() failed\n")
        smtp.state = SMTP_STATE_FAILED;
        return;
    }

    if (espconn_regist_connectcb(&smtp.conn, smtp_conn_cb)) {
        LOG_ERROR(SMTP, "espconn_regist_connectcb() failed\n");
        smtp.state = SMTP_STATE_FAILED;
        return;
    }

    if (espconn_regist_reconcb(&smtp.conn, smtp_error_cb)) {
        LOG_ERROR(SMTP, "espconn_regist_connectcb() failed\n");
        smtp.state = SMTP_STATE_FAILED;
        return;
    }

    if (!espconn_secure_connect(&smtp.conn)) {
        LOG_ERROR(SMTP, "espconn_secure_connect() failed\n");
        smtp.state = SMTP_STATE_FAILED;
        return;
    }

    /* FIXME STOPPED connect callback not being called */
}

ICACHE_FLASH_ATTR void smtp_conn_cb(void *arg) {
    struct espconn *conn = arg;

    LOG_DEBUG(SMTP, "connect: " IPSTR ":%u\n",
                    IP2STR(conn->proto.tcp->remote_ip),
                    conn->proto.tcp->remote_port)

    if (espconn_regist_disconcb(conn, smtp_disconn_cb)) {
        LOG_ERROR(HTTPD, "connect: espconn_regist_disconcb() failed\n")
        smtp.state = SMTP_STATE_FAILED;

        os_timer_disarm(&smtp.timer);
        os_timer_setfn(&smtp.timer, smtp_disconnect, NULL);
        os_timer_arm(&smtp.timer, 3000, false);

        return;
    }

    if (espconn_regist_recvcb(conn, smtp_recv_cb)) {
        LOG_ERROR(HTTPD, "connect: espconn_regist_recvcb() failed\n")
        smtp.state = SMTP_STATE_FAILED;

        os_timer_disarm(&smtp.timer);
        os_timer_setfn(&smtp.timer, smtp_disconnect, NULL);
        os_timer_arm(&smtp.timer, 3000, false);

        return;
    }

    smtp_send_greet();
}

ICACHE_FLASH_ATTR void smtp_send_greet() {
    uint16_t outbuflen;

    smtp.state = SMTP_STATE_GREET;

    /* The 127.0.1.1 is  not a typo, see https://serverfault.com/a/490530 */
    #define EHLO_LINE "ehlo [127.0.1.1]\r\n"

    if ((outbuflen = os_strlen(EHLO_LINE)) > sizeof(smtp.outbuf)) {
        LOG_ERROR(SMTP, "smtp outbuf overflow\n")
        smtp.state = SMTP_STATE_FAILED;
        smtp_disconnect();
        return;
    }
    os_memcpy(&smtp.outbuf, EHLO_LINE, outbuflen);

    if (espconn_secure_send(&smtp.conn, smtp.outbuf, outbuflen)) {
        LOG_ERROR(SMTP, "espconn_secure_send() failed\n")
        smtp.state = SMTP_STATE_FAILED;
        smtp_disconnect();
        return;
    }

    /* FIXME STOPPED */
    LOG_DEBUG(SMTP, ">>> %5s\n", "abcdefghijklmno")
    LOG_DEBUG(SMTP, ">>> %.5s\n", "abcdefghijklmno")
}

ICACHE_FLASH_ATTR void smtp_error_cb(void *arg, int8_t err) {
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

    /* TODO How should this be handled?  Likely dependent on the error */
}

ICACHE_FLASH_ATTR void smtp_disconn_cb(void *arg) {
    struct espconn *conn = arg;

    LOG_DEBUG(SMTP, "disconnect: " IPSTR ":%u\n",
                    IP2STR(conn->proto.tcp->remote_ip),
                    conn->proto.tcp->remote_port)

    smtp.state = SMTP_STATE_READY;
}

ICACHE_FLASH_ATTR void smtp_recv_cb(void *arg, char *data, unsigned short len) {
    struct espconn *conn = arg;

    LOG_DEBUG(SMTP, "recv: " IPSTR ":%u len=%u\n",
                    IP2STR(conn->proto.tcp->remote_ip),
                    conn->proto.tcp->remote_port, len)

    if (smtp.inbufused + len > sizeof(smtp.inbuf)) {
        LOG_WARNING(SMTP, "recv: smtp inbuf overflow\n")

        os_timer_disarm(&smtp.timer);
        os_timer_setfn(&smtp.timer, smtp_disconnect, NULL);
        os_timer_arm(&smtp.timer, 3000, false);

        return;
    }

    os_memcpy(smtp.inbuf + smtp.inbufused, data, len);
    smtp.inbufused += len;
}

ICACHE_FLASH_ATTR void smtp_disconnect() {
    LOG_DEBUG(SMTP, "disconnect: " IPSTR ":%u\n",
                    IP2STR(smtp.conn.proto.tcp->remote_ip),
                    smtp.conn.proto.tcp->remote_port)

    if (espconn_secure_disconnect(&smtp.conn))
        LOG_ERROR(SMTP, "espconn_secure_disconnect() failed\n");
}
