#include <osapi.h>

#include "../log.h"
#include "smtp.h"

static Smtp smtp;

static void smtp_conn_cb(void *arg);
static void smtp_error_cb(void *arg, int8_t err);

static void smtp_disconn_cb(void *arg);
static void smtp_recv_cb(void *arg, char *data, unsigned short len);

ICACHE_FLASH_ATTR void smtp_init_gmail(const char *user, const char *pass) {
    size_t len;

    #define GMAIL_HOST "smtp.gmail.com"
    #define GMAIL_PORT 587

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

ICACHE_FLASH_ATTR void smtp_send_dns(const char *host, ip_addr_t *ip,
                                     void *arg) {
    (void)host;
    (void)arg;

    if (ip == NULL) {
        LOG_INFO(SMTP, "smtp_send_dns resolution failed\n");
        smtp.state = SMTP_STATE_DNS_FAIL;
    }
    else {
        LOG_INFO(SMTP, "smtp_send_dns host=" IPSTR "\n", IP2STR(ip));
        smtp.state = SMTP_STATE_DNS_DONE;
        os_memcpy(&smtp.ip, ip, sizeof(smtp.ip));
    }
}

ICACHE_FLASH_ATTR int smtp_send(const char *to, const char *subj,
                                const char *body) {
    uint16_t i;

    if (smtp.state != SMTP_STATE_READY) {
        LOG_ERROR(SMTP, "not in ready state\n")
        return 1;
    }

    /* espconn_gethostbyname() seems to always return ESPCONN_INPROGRESS */
    espconn_gethostbyname(&smtp.conn, smtp.host, &smtp.ip, smtp_send_dns);

    for (i=0; i>10*1000; i++) {
        os_delay_us(1000);
        if (i>0 && i%1000==0)
            system_soft_wdt_feed();

        if (smtp.state == SMTP_STATE_DNS_FAIL)
            break;
        if (smtp.state == SMTP_STATE_DNS_DONE)
            break;
    }
    if (i == 10*1000+1) {
        LOG_ERROR(SMTP, "dns timeout\n")
        smtp.state = SMTP_STATE_READY;
        return 1;
    }

    os_bzero(&smtp.tcp, sizeof(smtp.tcp));
    smtp.tcp.remote_port = smtp.port;
    smtp.tcp.remote_ip[0] = smtp.ip.addr >> 24;
    smtp.tcp.remote_ip[1] = smtp.ip.addr >> 16;
    smtp.tcp.remote_ip[2] = smtp.ip.addr >> 8;
    smtp.tcp.remote_ip[3] = smtp.ip.addr;

    os_bzero(&smtp.conn, sizeof(smtp.conn));
    smtp.conn.type = ESPCONN_TCP;
    smtp.conn.proto.tcp = &smtp.tcp;

    if (espconn_regist_connectcb(&smtp.conn, smtp_conn_cb)) {
        LOG_ERROR(SMTP, "espconn_regist_connectcb() failed\n");
        return 1;
    }

    if (espconn_regist_reconcb(&smtp.conn, smtp_error_cb)) {
        LOG_ERROR(SMTP, "espconn_regist_connectcb() failed\n");
        return 1;
    }

    if (!espconn_secure_connect(&smtp.conn)) {
        LOG_ERROR(SMTP, "espconn_secure_connect() failed\n");
        return 1;
    }

    /* FIXME STOPPED */

    return 0;
}

ICACHE_FLASH_ATTR void smtp_conn_cb(void *arg) {
    struct espconn *conn = arg;

    LOG_DEBUG(SMTP, "connect: " IPSTR ":%u\n",
                    IP2STR(conn->proto.tcp->remote_ip),
                    conn->proto.tcp->remote_port)

    if (espconn_regist_disconcb(conn, smtp_disconn_cb)) {
        LOG_ERROR(HTTPD, "connect: espconn_regist_disconcb() failed\n")
        /* FIXME STOPPED Setup a timer to disconnect */
        return;
    }

    if (espconn_regist_recvcb(conn, smtp_recv_cb)) {
        LOG_ERROR(HTTPD, "connect: espconn_regist_recvcb() failed\n")
        /* FIXME STOPPED Setup a timer to disconnect */
        return;
    }

    /* FIXME Change smtp.state */
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

    /*
     * TODO How should this be handled?  Likely dependent on the error.
     *      Can verify this is the server socket by checking conn->state.
     *      Unsure if a espconn_disconnect() is needed or just espconn_delete().
     */
}

ICACHE_FLASH_ATTR void smtp_disconn_cb(void *arg) {
    /* FIXME */
}

ICACHE_FLASH_ATTR void smtp_recv_cb(void *arg, char *data, unsigned short len) {
    /* FIXME */
}
