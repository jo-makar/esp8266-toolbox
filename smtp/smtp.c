#include "../config.h"
#include "../log.h"
#include "../missing-decls.h"
#include "smtp.h"

#include <espconn.h>
#include <osapi.h>
#include <user_interface.h>

void smtp_handler(void *arg);
void smtp_resolve(const char *host, ip_addr_t *ip, void *arg);

void smtp_conn_cb(void *arg);
void smtp_error_cb(void *arg, int8_t err);
void smtp_disconn_cb(void *arg);
void smtp_recv_cb(void *arg, char *data, unsigned short len);
void smtp_sent_cb(void *arg);

void smtp_kill(void *arg);

struct {
    #define SMTP_STATE_ERROR   -1
    #define SMTP_STATE_READY    0
    #define SMTP_STATE_RESOLVE  1
    #define SMTP_STATE_CONNECT  2
    #define SMTP_STATE_GREET    3
    int8_t state;

    char to[128];
    char subj[128];
    char body[SMTP_STATE_BODYLEN];

    os_timer_t timer;

    struct espconn conn;
    esp_tcp tcp;

    ip_addr_t ip;

    os_timer_t ktimer;

    uint8_t inbuf[512];
    uint16_t inbufused;

    uint8_t outbuf[512];
    uint16_t outbufused;
} smtp_state;

ICACHE_FLASH_ATTR char *smtp_send_bodybuf() {
    return smtp_state.body;
}

ICACHE_FLASH_ATTR int8_t smtp_send_status() {
    return smtp_state.state;
}

ICACHE_FLASH_ATTR void smtp_send_reset() {
    if (smtp_state.state == SMTP_STATE_ERROR)
        smtp_state.state = SMTP_STATE_READY;
}

ICACHE_FLASH_ATTR void smtp_send_launch(const char *to, const char *subj,
                                        const char *body) {
    if (smtp_state.state != SMTP_STATE_READY) {
        log_error("smtp", "not in ready state");
        return;
    }

    if (to == NULL) {
        log_error("smtp", "param to == NULL");
        return;
    }
    if (os_strlen(to) >= sizeof(smtp_state.to)) {
        log_error("smtp", "param to overflow");
        return;
    }
    os_strcpy(smtp_state.to, to);

    smtp_state.subj[0] = 0;
    if (subj != NULL) {
        if (os_strlen(subj) >= sizeof(smtp_state.subj)) {
            log_error("smtp", "param subj overflow");
            return;
        }
        os_strcpy(smtp_state.subj, subj);
    }

    /* Ensure the buffer is null-terminated */
    smtp_state.body[SMTP_STATE_BODYLEN-1] = 0;
    if (body != NULL) {
        if (os_strlen(body) >= sizeof(smtp_state.body)) {
            log_error("smtp", "param body overflow");
            return;
        }
        os_strcpy(smtp_state.body, body);
    }

    /*
     * The SDK has no mechanism to "release" the processor to run other
     * tasks/timers (including for internal system tasks such as handling DNS
     * resolution responses), or to put it differently the scheduler only
     * triggers when nothing else is executing.
     *
     * Hence the process is done in stages with the handler self-scheduling
     * the next stage of execution as appropriate.
     */

    smtp_handler(NULL);
}

ICACHE_FLASH_ATTR void smtp_handler(void *arg) {
    (void)arg;

    log_debug("smtp", "smtp_handler: state=%d", smtp_state.state);

    if (smtp_state.state == SMTP_STATE_ERROR)
        return;

    else if (smtp_state.state == SMTP_STATE_READY) {
        err_t rv;

        rv = espconn_gethostbyname(&smtp_state.conn, SMTP_HOST, &smtp_state.ip,
                                   smtp_resolve);
        if (rv == ESPCONN_OK) {
            log_debug("smtp", "espconn_gethostbyname() => OK");
            /* smtp_state.ip is populated, immediately go to the next state */
            smtp_state.state = SMTP_STATE_CONNECT;
            smtp_handler(NULL);
            return;
        } else if (rv == ESPCONN_INPROGRESS) {
            log_debug("smtp", "espconn_gethostbyname() => INPROGRESS");
            /* smtp_resolve() will be called when resolution is done */
            return;
        } else {
            log_error("smtp", "espconn_gethostbyname() failed");
            smtp_state.state = SMTP_STATE_ERROR;
            return;
        }
    }

    else if (smtp_state.state == SMTP_STATE_CONNECT) {
        os_bzero(&smtp_state.tcp, sizeof(smtp_state.tcp));
        smtp_state.tcp.remote_port = SMTP_PORT;
        smtp_state.tcp.remote_ip[3] = smtp_state.ip.addr >> 24;
        smtp_state.tcp.remote_ip[2] = smtp_state.ip.addr >> 16;
        smtp_state.tcp.remote_ip[1] = smtp_state.ip.addr >> 8;
        smtp_state.tcp.remote_ip[0] = smtp_state.ip.addr;

        os_bzero(&smtp_state.conn, sizeof(smtp_state.conn));
        smtp_state.conn.type = ESPCONN_TCP;
        smtp_state.conn.state = ESPCONN_NONE;
        smtp_state.conn.proto.tcp = &smtp_state.tcp;

        if (espconn_regist_connectcb(&smtp_state.conn, smtp_conn_cb)) {
            log_error("smtp", "espconn_regist_connectcb() failed");
            smtp_state.state = SMTP_STATE_ERROR;
            return;
        }

        if (espconn_regist_reconcb(&smtp_state.conn, smtp_error_cb)) {
            log_error("smtp", "espconn_regist_reconcb() failed");
            smtp_state.state = SMTP_STATE_ERROR;
            return;
        }

        if (espconn_connect(&smtp_state.conn)) {
            log_error("smtp", "espconn_connect() failed");
            smtp_state.state = SMTP_STATE_ERROR;
            return;
        }

        return;
    }

    else if (smtp_state.state == SMTP_STATE_GREET) {
        // FIXME STOPPED
        return;
    }

    /* Schedule a re-execution in one second */
    os_timer_disarm(&smtp_state.timer);
    os_timer_setfn(&smtp_state.timer, smtp_handler, NULL);
    os_timer_arm(&smtp_state.timer, 1000, false);
}

ICACHE_FLASH_ATTR void smtp_resolve(const char *host, ip_addr_t *ip, void *arg) {
    (void)arg;

    /* Should never happen */
    if (os_strcmp(host, SMTP_HOST) != 0) {
        log_error("smtp", "host != SMTP_HOST");
        smtp_state.state = SMTP_STATE_ERROR;
        return;
    }

    if (ip == NULL) {
        log_error("smtp", "dns resolution failed");
        smtp_state.state = SMTP_STATE_ERROR;
    } else {
        log_info("smtp", "dns host=" IPSTR, IP2STR(ip));
        memcpy(&smtp_state.ip, ip, sizeof(ip_addr_t));
        smtp_state.state = SMTP_STATE_CONNECT;
        smtp_handler(NULL);
    }
}

ICACHE_FLASH_ATTR void smtp_conn_cb(void *arg) {
    struct espconn *conn = arg;

    log_debug("smtp", "connect: " IPSTR ":%u",
              IP2STR(conn->proto.tcp->remote_ip),
              conn->proto.tcp->remote_port);

    if (espconn_regist_disconcb(conn, smtp_disconn_cb)) {
        log_error("smtp", "espconn_regist_disconcb() failed");
        smtp_state.state = SMTP_STATE_ERROR;

        os_timer_disarm(&smtp_state.ktimer);
        os_timer_setfn(&smtp_state.ktimer, smtp_kill, conn);
        os_timer_arm(&smtp_state.ktimer, 100, false);

        return;
    }

    if (espconn_regist_recvcb(conn, smtp_recv_cb)) {
        log_error("smtp", "espconn_regist_recvcb() failed");
        smtp_state.state = SMTP_STATE_ERROR;

        os_timer_disarm(&smtp_state.ktimer);
        os_timer_setfn(&smtp_state.ktimer, smtp_kill, conn);
        os_timer_arm(&smtp_state.ktimer, 100, false);

        return;
    }

    if (espconn_regist_sentcb(conn, smtp_sent_cb)) {
        log_error("smtp", "espconn_regist_sentcb() failed");
        smtp_state.state = SMTP_STATE_ERROR;

        os_timer_disarm(&smtp_state.ktimer);
        os_timer_setfn(&smtp_state.ktimer, smtp_kill, conn);
        os_timer_arm(&smtp_state.ktimer, 100, false);

        return;
    }

    smtp_state.state = SMTP_STATE_GREET;

    os_timer_disarm(&smtp_state.timer);
    os_timer_setfn(&smtp_state.timer, smtp_handler, NULL);
    os_timer_arm(&smtp_state.timer, 100, false);
}

ICACHE_FLASH_ATTR void smtp_error_cb(void *arg, int8_t err) {
    struct espconn *conn = arg;
    char *type, buf[25];

    switch (err) {
        case ESPCONN_TIMEOUT:   type = "timeout";   break;
        case ESPCONN_ABRT:      type = "abrt";      break;
        case ESPCONN_RST:       type = "rst";       break;
        case ESPCONN_CLSD:      type = "clsd";      break;
        case ESPCONN_CONN:      type = "conn";      break;
        case ESPCONN_HANDSHAKE: type = "handshake"; break;

        default:
            os_sprintf(buf, "unknown (%02x)", err);
            type = buf;
            break;
    }

    log_error("smtp", "error: %s " IPSTR ":%u",
                      type, IP2STR(conn->proto.tcp->remote_ip),
                      conn->proto.tcp->remote_port);

    smtp_state.state = SMTP_STATE_ERROR;
}

ICACHE_FLASH_ATTR void smtp_disconn_cb(void *arg) {
    struct espconn *conn = arg;

    log_debug("smtp", "disconnect: " IPSTR ":%u",
              IP2STR(conn->proto.tcp->remote_ip),
              conn->proto.tcp->remote_port);
}

ICACHE_FLASH_ATTR void smtp_recv_cb(void *arg, char *data, unsigned short len) {
    struct espconn *conn = arg;

    log_debug("smtp", "recv: " IPSTR ":%u len=%u",
              IP2STR(conn->proto.tcp->remote_ip),
              conn->proto.tcp->remote_port, len);

    if (smtp_state.inbufused + len >= sizeof(smtp_state.inbuf)) {
        log_warning("smtp", "recv: smtp inbuf overflow");
        smtp_state.state = SMTP_STATE_ERROR;

        os_timer_disarm(&smtp_state.ktimer);
        os_timer_setfn(&smtp_state.ktimer, smtp_kill, conn);
        os_timer_arm(&smtp_state.ktimer, 100, false);

        return;
    }

    os_memcpy(smtp_state.inbuf + smtp_state.inbufused, data, len);
    smtp_state.inbufused += len;
}

ICACHE_FLASH_ATTR void smtp_sent_cb(void *arg) {
    struct espconn *conn = arg;

    log_debug("smtp", "sent: " IPSTR ":%u",
              IP2STR(conn->proto.tcp->remote_ip),
              conn->proto.tcp->remote_port);
}

ICACHE_FLASH_ATTR void smtp_kill(void *arg) {
    struct espconn *conn = arg;

    if (espconn_disconnect(conn))
        log_error("smtp", "kill: espconn_disconnect() failed");
    if (espconn_delete(conn))
        log_error("smtp", "kill: espconn_delete() failed");
}
