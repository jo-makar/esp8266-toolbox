#include "../crypto/base64.h"
#include "../crypto/md5.h"
#include "../log/log.h"
#include "../missing-decls.h"
#include "../param.h"
#include "private.h"

#include <osapi.h>

SmtpServer smtp_server;
static SmtpState smtp_state;

static void smtp_send_dns(const char *host, ip_addr_t *ip, void *arg);
static void smtp_send_connect(const ip_addr_t *ip);
static void smtp_send_greet();
static void smtp_send_challenge();
static void smtp_send_auth();
static void smtp_send_from();
static void smtp_send_to();
static void smtp_send_data();
static void smtp_send_quit();
static void smtp_send_close();

static void smtp_send_conn_cb(void *arg);
static void smtp_send_error_cb(void *arg, int8_t err);
static void smtp_send_disconn_cb(void *arg);
static void smtp_send_recv_cb(void *arg, char *data, unsigned short len);
static void smtp_send_sent_cb(void *arg);

static void smtp_send_kill(void *arg);

ICACHE_FLASH_ATTR void smtp_init() {
    os_bzero(&smtp_state, sizeof(smtp_state));
    smtp_state.state = SMTP_STATE_READY;

    os_bzero(&smtp_server, sizeof(smtp_server));
    if (param_retrieve(PARAM_SMTP_OFFSET, (uint8_t *)&smtp_server, sizeof(smtp_server)))
        return;

    DEBUG(SMTP, "host:port=%s:%u user=%s", smtp_server.host, smtp_server.port, smtp_server.user)
    DEBUG(SMTP, "from=%s to=%s", smtp_server.from, smtp_server.to)
}

ICACHE_FLASH_ATTR void smtp_send(const char *from, const char *to,
                                 const char *subj, const char *body) {
    ip_addr_t ip;
    err_t rv;

    if (os_strlen(smtp_server.host) == 0) {
        ERROR(SMTP, "no smtp account")
        return;
    }

    if (smtp_state.state != SMTP_STATE_READY) {
        ERROR(SMTP, "not in ready state")
        return;
    }

    if (os_strlen(from)+1 > sizeof(smtp_state.from)) {
        WARNING(SMTP, "from overflow")
        return;
    }
    os_strncpy((char *)smtp_state.from, from, os_strlen(from)+1);

    if (os_strlen(to)+1 > sizeof(smtp_state.to)) {
        WARNING(SMTP, "to overflow")
        return;
    }
    os_strncpy((char *)smtp_state.to, to, os_strlen(to)+1);

    if (os_strlen(subj)+1 > sizeof(smtp_state.subj)) {
        WARNING(SMTP, "subj overflow")
        return;
    }
    os_strncpy((char *)smtp_state.subj, subj, os_strlen(subj)+1);

    if (os_strlen(body)+1 > sizeof(smtp_state.body)) {
        WARNING(SMTP, "body overflow")
        return;
    }
    os_strncpy((char *)smtp_state.body, body, os_strlen(body)+1);

    /*
     * Nothing (not even os_delay_us()) "releases" the process to run tasks,
     * including internal system tasks such as handling DNS resolution responses.
     * Or to put it differently no task/timer can preempt another task/timer.
     *
     * Hence this process is daisy-chained from a successful DNS resolution.
     */

    smtp_state.state = SMTP_STATE_RESOLVE;
    rv = espconn_gethostbyname(&smtp_state.conn, smtp_server.host, &ip, smtp_send_dns);
    if (rv == ESPCONN_OK) {
        DEBUG(SMTP, "espconn_gethostbyname() => OK")
        smtp_send_connect(&ip);
    } else if (rv == ESPCONN_INPROGRESS) {
        DEBUG(SMTP, "espconn_gethostbyname() => INPROGRESS")
    } else {
        ERROR(SMTP, "espconn_gethostbyname() failed")
        smtp_state.state = SMTP_STATE_FAILED;
    }

}

ICACHE_FLASH_ATTR void smtp_send_dns(const char *host, ip_addr_t *ip, void *arg) {
    (void)host;
    (void)arg;

    if (ip == NULL) {
        ERROR(SMTP, "dns resolution failed")
        smtp_state.state = SMTP_STATE_FAILED;
    } else {
        INFO(SMTP, "dns host=" IPSTR, IP2STR(ip))
        smtp_send_connect(ip);
    }
}

ICACHE_FLASH_ATTR void smtp_send_connect(const ip_addr_t *ip) {
    smtp_state.state = SMTP_STATE_CONNECT;

    os_bzero(&smtp_state.tcp, sizeof(smtp_state.tcp));
    smtp_state.tcp.remote_port = smtp_server.port;
    smtp_state.tcp.remote_ip[3] = ip->addr >> 24;
    smtp_state.tcp.remote_ip[2] = ip->addr >> 16;
    smtp_state.tcp.remote_ip[1] = ip->addr >> 8;
    smtp_state.tcp.remote_ip[0] = ip->addr;

    os_bzero(&smtp_state.conn, sizeof(smtp_state.conn));
    smtp_state.conn.type = ESPCONN_TCP;
    smtp_state.conn.state = ESPCONN_NONE;
    smtp_state.conn.proto.tcp = &smtp_state.tcp;

    if (espconn_regist_connectcb(&smtp_state.conn, smtp_send_conn_cb)) {
        ERROR(SMTP, "espconn_regist_connectcb() failed")
        smtp_state.state = SMTP_STATE_FAILED;
        return;
    }

    if (espconn_regist_reconcb(&smtp_state.conn, smtp_send_error_cb)) {
        ERROR(SMTP, "espconn_regist_reconcb() failed")
        smtp_state.state = SMTP_STATE_FAILED;
        return;
    }

    if (espconn_connect(&smtp_state.conn)) {
        ERROR(SMTP, "espconn_connect() failed")
        smtp_state.state = SMTP_STATE_FAILED;
        return;
    }
}

ICACHE_FLASH_ATTR void smtp_send_conn_cb(void *arg) {
    struct espconn *conn = arg;

    DEBUG(SMTP, "connect: " IPSTR ":%u",
                IP2STR(conn->proto.tcp->remote_ip),
                conn->proto.tcp->remote_port)

    if (espconn_regist_disconcb(conn, smtp_send_disconn_cb)) {
        ERROR(SMTP, "espconn_regist_disconcb() failed")
        smtp_state.state = SMTP_STATE_FAILED;

        os_timer_disarm(&smtp_state.timer);
        os_timer_setfn(&smtp_state.timer, smtp_send_kill, conn);
        os_timer_arm(&smtp_state.timer, 3000, false);

        return;
    }

    if (espconn_regist_recvcb(conn, smtp_send_recv_cb)) {
        ERROR(SMTP, "espconn_regist_recvcb() failed")
        smtp_state.state = SMTP_STATE_FAILED;

        os_timer_disarm(&smtp_state.timer);
        os_timer_setfn(&smtp_state.timer, smtp_send_kill, conn);
        os_timer_arm(&smtp_state.timer, 3000, false);

        return;
    }

    if (espconn_regist_sentcb(conn, smtp_send_sent_cb)) {
        ERROR(SMTP, "espconn_regist_sentcb() failed")
        smtp_state.state = SMTP_STATE_FAILED;

        os_timer_disarm(&smtp_state.timer);
        os_timer_setfn(&smtp_state.timer, smtp_send_kill, conn);
        os_timer_arm(&smtp_state.timer, 3000, false);

        return;
    }

    smtp_state.state = SMTP_STATE_GREET;

    if (espconn_send(&smtp_state.conn, (uint8_t *)"ehlo [127.0.1.1]\r\n", 18)) {
        ERROR(SMTP, "espconn_send() failed")
        smtp_state.state = SMTP_STATE_FAILED;

        os_timer_disarm(&smtp_state.timer);
        os_timer_setfn(&smtp_state.timer, smtp_send_kill, NULL);
        os_timer_arm(&smtp_state.timer, 3000, false);

        return;
    }
    DEBUG(SMTP, ">> ehlo [127.0.1.1]")
}

ICACHE_FLASH_ATTR void smtp_send_error_cb(void *arg, int8_t err) {
    struct espconn *conn = arg;

    switch (err) {
        case ESPCONN_TIMEOUT:
            ERROR(SMTP, "error: timeout " IPSTR ":%u",
                        IP2STR(conn->proto.tcp->remote_ip),
                        conn->proto.tcp->remote_port)
            break;
        case ESPCONN_ABRT:
            ERROR(SMTP, "error: abrt " IPSTR ":%u",
                        IP2STR(conn->proto.tcp->remote_ip),
                        conn->proto.tcp->remote_port)
            break;
        case ESPCONN_RST:
            ERROR(SMTP, "error: rst " IPSTR ":%u",
                        IP2STR(conn->proto.tcp->remote_ip),
                        conn->proto.tcp->remote_port)
            break;
        case ESPCONN_CLSD:
            ERROR(SMTP, "error: clsd " IPSTR ":%u",
                        IP2STR(conn->proto.tcp->remote_ip),
                        conn->proto.tcp->remote_port)
            break;
        case ESPCONN_CONN:
            ERROR(SMTP, "error: conn " IPSTR ":%u",
                        IP2STR(conn->proto.tcp->remote_ip),
                        conn->proto.tcp->remote_port)
            break;
        case ESPCONN_HANDSHAKE:
            ERROR(SMTP, "error: handshake " IPSTR ":%u",
                        IP2STR(conn->proto.tcp->remote_ip),
                        conn->proto.tcp->remote_port)
            break;
        default:
            ERROR(SMTP, "error: unknown (%02x) " IPSTR ":%u",
                        err, IP2STR(conn->proto.tcp->remote_ip),
                        conn->proto.tcp->remote_port)
            break;
    }
}

ICACHE_FLASH_ATTR void smtp_send_disconn_cb(void *arg) {
    struct espconn *conn = arg;

    DEBUG(SMTP, "disconnect: " IPSTR ":%u",
                IP2STR(conn->proto.tcp->remote_ip),
                conn->proto.tcp->remote_port)
}

ICACHE_FLASH_ATTR void smtp_send_recv_cb(void *arg, char *data, unsigned short len) {
    struct espconn *conn = arg;

    DEBUG(SMTP, "recv: " IPSTR ":%u len=%u",
                IP2STR(conn->proto.tcp->remote_ip),
                conn->proto.tcp->remote_port, len)

    if (smtp_state.inbufused + len >= sizeof(smtp_state.inbuf)) {
        WARNING(SMTP, "recv: smtp inbuf overflow")
        smtp_state.state = SMTP_STATE_FAILED;

        os_timer_disarm(&smtp_state.timer);
        os_timer_setfn(&smtp_state.timer, smtp_send_kill, NULL);
        os_timer_arm(&smtp_state.timer, 3000, false);

        return;
    }

    os_memcpy(smtp_state.inbuf + smtp_state.inbufused, data, len);
    smtp_state.inbufused += len;

    if (smtp_state.state == SMTP_STATE_GREET)
        smtp_send_greet();
    else if (smtp_state.state == SMTP_STATE_CHALLENGE)
        smtp_send_challenge();
    else if (smtp_state.state == SMTP_STATE_AUTH)
        smtp_send_auth();
    else if (smtp_state.state == SMTP_STATE_FROM)
        smtp_send_from();
    else if (smtp_state.state == SMTP_STATE_TO)
        smtp_send_to();
    else if (smtp_state.state == SMTP_STATE_DATA)
        smtp_send_data();
    else if (smtp_state.state == SMTP_STATE_QUIT)
        smtp_send_quit();
    else if (smtp_state.state == SMTP_STATE_CLOSE)
        smtp_send_close();
}

ICACHE_FLASH_ATTR void smtp_send_sent_cb(void *arg) {
    struct espconn *conn = arg;

    DEBUG(SMTP, "sent: " IPSTR ":%u",
                IP2STR(conn->proto.tcp->remote_ip),
                conn->proto.tcp->remote_port)
}

ICACHE_FLASH_ATTR void smtp_send_kill(void *arg) {
    struct espconn *conn = arg;

    if (espconn_disconnect(conn))
        ERROR(SMTP, "kill: espconn_disconnect() failed")
    if (espconn_delete(conn))
        ERROR(SMTP, "kill: espconn_delete() failed")
}

ICACHE_FLASH_ATTR void smtp_send_greet() {
    uint8_t *end;
    uint8_t good, done;

    smtp_state.inbuf[os_strlen((char *)smtp_state.inbuf)] = 0;

    while (1) {
        if ((end = (uint8_t *)os_strstr((char *)smtp_state.inbuf, "\r\n")) == NULL)
            return;

        *end = 0;
        DEBUG(SMTP, "<< %s", smtp_state.inbuf)
        *end = '\r';

        good = 0;
        if (os_strncmp("220", (char *)smtp_state.inbuf, 3) == 0)
            good = 1;
        else if (os_strncmp("250", (char *)smtp_state.inbuf, 3) == 0)
            good = 1;

        if (!good) {
            ERROR(SMTP, "non 220/250 response")
            smtp_state.state = SMTP_STATE_FAILED;

            os_timer_disarm(&smtp_state.timer);
            os_timer_setfn(&smtp_state.timer, smtp_send_kill, NULL);
            os_timer_arm(&smtp_state.timer, 3000, false);

            return;
        }

        done = 0;
        if (os_strncmp("250 ", (char *)smtp_state.inbuf, 4) == 0)
            done = 1;

        end += 2;
        smtp_state.inbufused -= end-smtp_state.inbuf;
        os_memmove(smtp_state.inbuf, end, smtp_state.inbufused+1);

        if (done)
            break;

        if (smtp_state.inbufused == 0)
            return;
    }

    smtp_state.state = SMTP_STATE_CHALLENGE;

    if (espconn_send(&smtp_state.conn, (uint8_t *)"AUTH CRAM-MD5\r\n", 15)) {
        ERROR(SMTP, "espconn_send() failed")
        smtp_state.state = SMTP_STATE_FAILED;

        os_timer_disarm(&smtp_state.timer);
        os_timer_setfn(&smtp_state.timer, smtp_send_kill, NULL);
        os_timer_arm(&smtp_state.timer, 3000, false);

        return;
    }
    DEBUG(SMTP, ">> AUTH CRAM-MD5")
}

ICACHE_FLASH_ATTR void smtp_send_challenge() {
    #define MD5_BLOCKSIZE 64
    #define MD5_HASHLEN 16

    uint8_t chal[128];
    uint8_t resp[MD5_HASHLEN];
    uint8_t *end;
    size_t len;
    int len2;

    smtp_state.inbuf[os_strlen((char *)smtp_state.inbuf)] = 0;

    if ((end = (uint8_t *)os_strstr((char *)smtp_state.inbuf, "\r\n")) == NULL)
        return;

    *end = 0;
    DEBUG(SMTP, "<< %s", smtp_state.inbuf)
    *end = '\r';

    if (os_strncmp("334 ", (char *)smtp_state.inbuf, 4) != 0) {
        ERROR(SMTP, "non 334 response")
        smtp_state.state = SMTP_STATE_FAILED;

        os_timer_disarm(&smtp_state.timer);
        os_timer_setfn(&smtp_state.timer, smtp_send_kill, NULL);
        os_timer_arm(&smtp_state.timer, 3000, false);

        return;
    }

    len = end - (smtp_state.inbuf+4);
    if ((len2 = b64_decode(smtp_state.inbuf+4, len, chal, sizeof(chal))) < 0) {
        smtp_state.state = SMTP_STATE_FAILED;

        os_timer_disarm(&smtp_state.timer);
        os_timer_setfn(&smtp_state.timer, smtp_send_kill, NULL);
        os_timer_arm(&smtp_state.timer, 3000, false);

        return;
    }
    chal[len2] = 0;
    DEBUG(SMTP, "chal = %s", chal)

    end += 2;
    smtp_state.inbufused -= end-smtp_state.inbuf;
    os_memmove(smtp_state.inbuf, end, smtp_state.inbufused+1);

    smtp_state.state = SMTP_STATE_AUTH;

    {
        uint8_t key[MD5_BLOCKSIZE];
        uint8_t left[MD5_BLOCKSIZE];
        uint8_t right[256];
        size_t i;

        os_bzero(key, sizeof(key));
        if ((len = os_strlen(smtp_server.pass)) > MD5_BLOCKSIZE)
            md5((uint8_t *)smtp_server.pass, len, key);
        else
            os_memcpy(key, smtp_server.pass, len);

        /* Ref: https://en.wikipedia.org/wiki/Hash-based_message_authentication_code#Definition */

        os_memcpy(left, key, MD5_BLOCKSIZE);
        for (i=0; i<MD5_BLOCKSIZE; i++)
            left[i] ^= 0x5c;

        DEBUG(SMTP, "left = %02x%02x%02x%02x%02x%02x%02x%02x"
                           "%02x%02x%02x%02x%02x%02x%02x%02x",
              left[0],left[1],left[2], left[3], left[4], left[5], left[6], left[7],
              left[8],left[9],left[10],left[11],left[12],left[13],left[14],left[15])

        len = os_strlen((char *)chal);
        if (MD5_BLOCKSIZE + len > sizeof(right)) {
            ERROR(SMTP, "right overflow")
            smtp_state.state = SMTP_STATE_FAILED;

            os_timer_disarm(&smtp_state.timer);
            os_timer_setfn(&smtp_state.timer, smtp_send_kill, NULL);
            os_timer_arm(&smtp_state.timer, 3000, false);

            return;
        }

        os_memcpy(right, key, MD5_BLOCKSIZE);
        for (i=0; i<MD5_BLOCKSIZE; i++)
            right[i] ^= 0x36;
        os_memcpy(right+MD5_BLOCKSIZE, chal, len);

        DEBUG(SMTP, "right = %02x%02x%02x%02x%02x%02x%02x%02x"
                            "%02x%02x%02x%02x%02x%02x%02x%02x%s",
              right[0],right[1],right[2], right[3], right[4], right[5], right[6], right[7],
              right[8],right[9],right[10],right[11],right[12],right[13],right[14],right[15],
              right+16)

        /* Reusing right as a tmp buffer */

        md5(right, MD5_BLOCKSIZE + len, right+MD5_BLOCKSIZE);
        DEBUG(SMTP, "md5(right) = %02x%02x%02x%02x%02x%02x%02x%02x"
                                 "%02x%02x%02x%02x%02x%02x%02x%02x",
              right[64+0],right[64+1],right[64+2], right[64+3], right[64+4], right[64+5], right[64+6], right[64+7],
              right[64+8],right[64+9],right[64+10],right[64+11],right[64+12],right[64+13],right[64+14],right[64+15])

        os_memcpy(right, left, MD5_BLOCKSIZE);
        md5(right, MD5_BLOCKSIZE + MD5_HASHLEN, resp);

        DEBUG(SMTP, "resp = %02x%02x%02x%02x%02x%02x%02x%02x"
                           "%02x%02x%02x%02x%02x%02x%02x%02x",
              resp[0],resp[1],resp[2], resp[3], resp[4], resp[5], resp[6], resp[7],
              resp[8],resp[9],resp[10],resp[11],resp[12],resp[13],resp[14],resp[15])

        len = os_strlen(smtp_server.user) + 33;
        if (len > sizeof(right)) {
            ERROR(SMTP, "right overflow")
            smtp_state.state = SMTP_STATE_FAILED;

            os_timer_disarm(&smtp_state.timer);
            os_timer_setfn(&smtp_state.timer, smtp_send_kill, NULL);
            os_timer_arm(&smtp_state.timer, 3000, false);

            return;
        }
        os_snprintf((char *)right, sizeof(right),
                    "%s %02x%02x%02x%02x%02x%02x%02x%02x"
                       "%02x%02x%02x%02x%02x%02x%02x%02x",
                    smtp_server.user,
                    resp[0],resp[1],resp[2], resp[3], resp[4], resp[5], resp[6], resp[7],
                    resp[8],resp[9],resp[10],resp[11],resp[12],resp[13],resp[14],resp[15]);

        DEBUG(SMTP, "%s", right)

        if ((len2 = b64_encode(right, len, smtp_state.outbuf,
                               sizeof(smtp_state.outbuf))) < 0) {
            smtp_state.state = SMTP_STATE_FAILED;

            os_timer_disarm(&smtp_state.timer);
            os_timer_setfn(&smtp_state.timer, smtp_send_kill, NULL);
            os_timer_arm(&smtp_state.timer, 3000, false);

            return;
        }

        os_memcpy(smtp_state.outbuf+len2, "\r\n", 2);
        len2 += 2;
    }
    
    if (espconn_send(&smtp_state.conn, smtp_state.outbuf, len2)) {
        ERROR(SMTP, "espconn_send() failed")
        smtp_state.state = SMTP_STATE_FAILED;

        os_timer_disarm(&smtp_state.timer);
        os_timer_setfn(&smtp_state.timer, smtp_send_kill, NULL);
        os_timer_arm(&smtp_state.timer, 3000, false);

        return;
    }

    smtp_state.outbuf[len2-2] = 0;
    DEBUG(SMTP, ">> %s", smtp_state.outbuf)
}

ICACHE_FLASH_ATTR void smtp_send_auth() {
    uint8_t *end;

    smtp_state.inbuf[os_strlen((char *)smtp_state.inbuf)] = 0;

    if ((end = (uint8_t *)os_strstr((char *)smtp_state.inbuf, "\r\n")) == NULL)
        return;

    *end = 0;
    DEBUG(SMTP, "<< %s", smtp_state.inbuf)
    *end = '\r';

    if (os_strncmp("235 ", (char *)smtp_state.inbuf, 4) != 0) {
        ERROR(SMTP, "non 235 response")
        smtp_state.state = SMTP_STATE_FAILED;

        os_timer_disarm(&smtp_state.timer);
        os_timer_setfn(&smtp_state.timer, smtp_send_kill, NULL);
        os_timer_arm(&smtp_state.timer, 3000, false);

        return;
    }

    end += 2;
    smtp_state.inbufused -= end-smtp_state.inbuf;
    os_memmove(smtp_state.inbuf, end, smtp_state.inbufused+1);

    smtp_state.state = SMTP_STATE_FROM;

    os_snprintf((char *)smtp_state.outbuf, sizeof(smtp_state.outbuf), "mail FROM:<%s>\r\n", smtp_state.from);

    if (espconn_send(&smtp_state.conn, smtp_state.outbuf, os_strlen((char *)smtp_state.from) + 14)) {
        ERROR(SMTP, "espconn_send() failed")
        smtp_state.state = SMTP_STATE_FAILED;

        os_timer_disarm(&smtp_state.timer);
        os_timer_setfn(&smtp_state.timer, smtp_send_kill, NULL);
        os_timer_arm(&smtp_state.timer, 3000, false);

        return;
    }

    smtp_state.outbuf[os_strlen((char *)smtp_state.from) + 12] = 0;
    DEBUG(SMTP, ">> %s", smtp_state.outbuf)
}

ICACHE_FLASH_ATTR void smtp_send_from() {
    uint8_t *end;

    smtp_state.inbuf[os_strlen((char *)smtp_state.inbuf)] = 0;

    if ((end = (uint8_t *)os_strstr((char *)smtp_state.inbuf, "\r\n")) == NULL)
        return;

    *end = 0;
    DEBUG(SMTP, "<< %s", smtp_state.inbuf)
    *end = '\r';

    if (os_strncmp("250 ", (char *)smtp_state.inbuf, 4) != 0) {
        ERROR(SMTP, "non 250 response")
        smtp_state.state = SMTP_STATE_FAILED;

        os_timer_disarm(&smtp_state.timer);
        os_timer_setfn(&smtp_state.timer, smtp_send_kill, NULL);
        os_timer_arm(&smtp_state.timer, 3000, false);

        return;
    }

    end += 2;
    smtp_state.inbufused -= end-smtp_state.inbuf;
    os_memmove(smtp_state.inbuf, end, smtp_state.inbufused+1);

    smtp_state.state = SMTP_STATE_TO;

    os_snprintf((char *)smtp_state.outbuf, sizeof(smtp_state.outbuf), "rcpt TO:<%s>\r\n", smtp_state.to);

    if (espconn_send(&smtp_state.conn, smtp_state.outbuf, os_strlen((char *)smtp_state.to) + 12)) {
        ERROR(SMTP, "espconn_send() failed")
        smtp_state.state = SMTP_STATE_FAILED;

        os_timer_disarm(&smtp_state.timer);
        os_timer_setfn(&smtp_state.timer, smtp_send_kill, NULL);
        os_timer_arm(&smtp_state.timer, 3000, false);

        return;
    }

    smtp_state.outbuf[os_strlen((char *)smtp_state.to) + 10] = 0;
    DEBUG(SMTP, ">> %s", smtp_state.outbuf)
}

ICACHE_FLASH_ATTR void smtp_send_to() {
    uint8_t *end;

    smtp_state.inbuf[os_strlen((char *)smtp_state.inbuf)] = 0;

    if ((end = (uint8_t *)os_strstr((char *)smtp_state.inbuf, "\r\n")) == NULL)
        return;

    *end = 0;
    DEBUG(SMTP, "<< %s", smtp_state.inbuf)
    *end = '\r';

    if (os_strncmp("250 ", (char *)smtp_state.inbuf, 4) != 0) {
        ERROR(SMTP, "non 250 response")
        smtp_state.state = SMTP_STATE_FAILED;

        os_timer_disarm(&smtp_state.timer);
        os_timer_setfn(&smtp_state.timer, smtp_send_kill, NULL);
        os_timer_arm(&smtp_state.timer, 3000, false);

        return;
    }

    end += 2;
    smtp_state.inbufused -= end-smtp_state.inbuf;
    os_memmove(smtp_state.inbuf, end, smtp_state.inbufused+1);

    smtp_state.state = SMTP_STATE_DATA;

    os_snprintf((char *)smtp_state.outbuf, sizeof(smtp_state.outbuf), "data\r\n");

    if (espconn_send(&smtp_state.conn, smtp_state.outbuf, 6)) {
        ERROR(SMTP, "espconn_send() failed")
        smtp_state.state = SMTP_STATE_FAILED;

        os_timer_disarm(&smtp_state.timer);
        os_timer_setfn(&smtp_state.timer, smtp_send_kill, NULL);
        os_timer_arm(&smtp_state.timer, 3000, false);

        return;
    }

    DEBUG(SMTP, ">> data")
}

ICACHE_FLASH_ATTR void smtp_send_data() {
    uint8_t *end;

    smtp_state.inbuf[os_strlen((char *)smtp_state.inbuf)] = 0;

    if ((end = (uint8_t *)os_strstr((char *)smtp_state.inbuf, "\r\n")) == NULL)
        return;

    *end = 0;
    DEBUG(SMTP, "<< %s", smtp_state.inbuf)
    *end = '\r';

    if (os_strncmp("354 ", (char *)smtp_state.inbuf, 4) != 0) {
        ERROR(SMTP, "non 354 response")
        smtp_state.state = SMTP_STATE_FAILED;

        os_timer_disarm(&smtp_state.timer);
        os_timer_setfn(&smtp_state.timer, smtp_send_kill, NULL);
        os_timer_arm(&smtp_state.timer, 3000, false);

        return;
    }

    end += 2;
    smtp_state.inbufused -= end-smtp_state.inbuf;
    os_memmove(smtp_state.inbuf, end, smtp_state.inbufused+1);

    smtp_state.state = SMTP_STATE_QUIT;

    /*
     * Should check for a single dot on a line in the body,
     * if found replace with two dots (called dot stuffing).
     */


    os_snprintf((char *)smtp_state.outbuf, sizeof(smtp_state.outbuf),
                "From: %s\r\nTo: %s\r\nSubject: %s\r\n\r\n%s\r\n.\r\n",
                smtp_state.from, smtp_state.to, smtp_state.subj, smtp_state.body);

    if (espconn_send(&smtp_state.conn, smtp_state.outbuf,
                      6 + os_strlen((char *)smtp_state.from) +
                      6 + os_strlen((char *)smtp_state.to)   +
                     11 + os_strlen((char *)smtp_state.subj) +
                      4 + os_strlen((char *)smtp_state.body) + 5)) {
        ERROR(SMTP, "espconn_send() failed")
        smtp_state.state = SMTP_STATE_FAILED;

        os_timer_disarm(&smtp_state.timer);
        os_timer_setfn(&smtp_state.timer, smtp_send_kill, NULL);
        os_timer_arm(&smtp_state.timer, 3000, false);

        return;
    }

    INFO(SMTP, "from=%s", smtp_state.from)
    INFO(SMTP, "  to=%s", smtp_state.to)
    INFO(SMTP, "subj=%s", smtp_state.subj)
    INFO(SMTP, "body=%s", smtp_state.body)
}

ICACHE_FLASH_ATTR void smtp_send_quit() {
    uint8_t *end;

    smtp_state.inbuf[os_strlen((char *)smtp_state.inbuf)] = 0;

    if ((end = (uint8_t *)os_strstr((char *)smtp_state.inbuf, "\r\n")) == NULL)
        return;

    *end = 0;
    DEBUG(SMTP, "<< %s", smtp_state.inbuf)
    *end = '\r';

    if (os_strncmp("250 ", (char *)smtp_state.inbuf, 4) != 0) {
        ERROR(SMTP, "non 250 response")
        smtp_state.state = SMTP_STATE_FAILED;

        os_timer_disarm(&smtp_state.timer);
        os_timer_setfn(&smtp_state.timer, smtp_send_kill, NULL);
        os_timer_arm(&smtp_state.timer, 3000, false);

        return;
    }

    end += 2;
    smtp_state.inbufused -= end-smtp_state.inbuf;
    os_memmove(smtp_state.inbuf, end, smtp_state.inbufused+1);

    smtp_state.state = SMTP_STATE_CLOSE;

    os_snprintf((char *)smtp_state.outbuf, sizeof(smtp_state.outbuf), "quit\r\n");

    if (espconn_send(&smtp_state.conn, smtp_state.outbuf, 6)) {
        ERROR(SMTP, "espconn_send() failed")
        smtp_state.state = SMTP_STATE_FAILED;

        os_timer_disarm(&smtp_state.timer);
        os_timer_setfn(&smtp_state.timer, smtp_send_kill, NULL);
        os_timer_arm(&smtp_state.timer, 3000, false);

        return;
    }

    DEBUG(SMTP, ">> quit")
}

ICACHE_FLASH_ATTR void smtp_send_close() {
    uint8_t *end;

    smtp_state.inbuf[os_strlen((char *)smtp_state.inbuf)] = 0;

    if ((end = (uint8_t *)os_strstr((char *)smtp_state.inbuf, "\r\n")) == NULL)
        return;

    *end = 0;
    DEBUG(SMTP, "<< %s", smtp_state.inbuf)
    *end = '\r';

    if (os_strncmp("221 ", (char *)smtp_state.inbuf, 4) != 0) {
        ERROR(SMTP, "non 221 response")
        smtp_state.state = SMTP_STATE_FAILED;

        os_timer_disarm(&smtp_state.timer);
        os_timer_setfn(&smtp_state.timer, smtp_send_kill, NULL);
        os_timer_arm(&smtp_state.timer, 3000, false);

        return;
    }

    end += 2;
    smtp_state.inbufused -= end-smtp_state.inbuf;
    os_memmove(smtp_state.inbuf, end, smtp_state.inbufused+1);

    smtp_state.state = SMTP_STATE_READY;
}
