#include "../crypto/base64.h"
#include "../crypto/md5.h"
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

#define SMTP_ABORT(conn) { \
    smtp_state.state = SMTP_STATE_ERROR; \
    os_timer_disarm(&smtp_state.ktimer); \
    os_timer_setfn(&smtp_state.ktimer, smtp_kill, conn); \
    os_timer_arm(&smtp_state.ktimer, 100, false); \
    return; \
}

struct {
    #define SMTP_STATE_ERROR      -1
    #define SMTP_STATE_READY       0
    #define SMTP_STATE_RESOLVE     1
    #define SMTP_STATE_CONNECT     2
    #define SMTP_STATE_GREET       3
    #define SMTP_STATE_GREET_RESP  4
    #define SMTP_STATE_AUTH_RESP   5
    #define SMTP_STATE_CHAL_RESP   6
    #define SMTP_STATE_FROM_RESP   7
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
        #define OUT "EHLO [127.0.1.1]"
        #define LEN os_strlen(OUT)

        if (espconn_send(&smtp_state.conn,
                         (uint8_t *)OUT "\r\n", LEN+2)) {
            log_error("smtp", "espconn_send() failed");
            SMTP_ABORT(&smtp_state.conn)
        }
        log_debug("smtp", ">> " OUT);

        smtp_state.state = SMTP_STATE_GREET_RESP;

        #undef OUT
        #undef LEN
    }

    else if (smtp_state.state == SMTP_STATE_GREET_RESP) {
        uint8_t *end;
        uint8_t good, done;

        smtp_state.inbuf[smtp_state.inbufused] = 0;

        done = 0;
        while (!done) {
            end = (uint8_t *)os_strstr((char *)smtp_state.inbuf, "\r\n");
            if (end == NULL)
                break;

            *end = 0;
            log_debug("smtp", "<< %s", smtp_state.inbuf);
            *end = '\r';

            good = 0;
            if (os_strncmp("220", (char *)smtp_state.inbuf, 3) == 0)
                good = 1;
            else if (os_strncmp("250", (char *)smtp_state.inbuf, 3) == 0)
                good = 1;

            if (!good) {
                log_error("smtp", "non 220/250 response");
                SMTP_ABORT(&smtp_state.conn)
            }

            if (os_strncmp("250 ", (char *)smtp_state.inbuf, 4) == 0)
                done = 1;

            end += 2;
            smtp_state.inbufused -= end - smtp_state.inbuf;
            os_memmove(smtp_state.inbuf, end, smtp_state.inbufused+1);
        }

        if (done) {
            #define OUT "AUTH CRAM-MD5"
            #define LEN os_strlen(OUT)

            if (espconn_send(&smtp_state.conn,
                             (uint8_t *)OUT "\r\n", LEN+2)) {
                log_error("smtp", "espconn_send() failed");
                SMTP_ABORT(&smtp_state.conn)
            }
            log_debug("smtp", ">> " OUT);

            smtp_state.state = SMTP_STATE_AUTH_RESP;

            #undef OUT
            #undef LEN
        }
    }

    else if (smtp_state.state == SMTP_STATE_AUTH_RESP) {
        uint8_t chal[128];
        uint8_t *end;
        size_t  len;
        int     len2;

        smtp_state.inbuf[smtp_state.inbufused] = 0;

        end = (uint8_t *)os_strstr((char *)smtp_state.inbuf, "\r\n");
        if (end != NULL) {
            *end = 0;
            log_debug("smtp", "<< %s", smtp_state.inbuf);
            *end = '\r';
            
            if (os_strncmp("334 ", (char *)smtp_state.inbuf, 4) != 0) {
                log_error("smtp", "non 334 response");
                SMTP_ABORT(&smtp_state.conn)
            }

            len  = end - (smtp_state.inbuf+4);
            len2 = base64_decode(smtp_state.inbuf+4, len, chal, sizeof(chal));
            if (len2 < 0)
                SMTP_ABORT(&smtp_state.conn)
            chal[len2] = 0;
            log_debug("smtp", "chal = %s", chal);

            end += 2;
            smtp_state.inbufused -= end - smtp_state.inbuf;
            os_memmove(smtp_state.inbuf, end, smtp_state.inbufused+1);

            {
                uint8_t resp[MD5_BLOCKSIZE];
                uint8_t key[MD5_BLOCKSIZE];
                uint8_t left[MD5_BLOCKSIZE];
                uint8_t right[256];
                size_t  i;

                os_bzero(key, sizeof(key));
                if ((len = os_strlen(SMTP_PASS)) > MD5_BLOCKSIZE)
                    md5((uint8_t *)SMTP_PASS, len, key);
                else
                    os_memcpy(key, SMTP_PASS, len);

                /* Ref: https://en.wikipedia.org/wiki/Hash-based_message_authentication_code#Definition */

                os_memcpy(left, key, MD5_BLOCKSIZE);
                for (i=0; i<MD5_BLOCKSIZE; i++)
                    left[i] ^= 0x5c;

                log_debug("smtp", "left = %02x%02x%02x%02x%02x%02x%02x%02x"
                                         "%02x%02x%02x%02x%02x%02x%02x%02x",
                                   left[0],left[1],left[2], left[3], left[4], left[5], left[6], left[7],
                                   left[8],left[9],left[10],left[11],left[12],left[13],left[14],left[15]);

                len = os_strlen((char *)chal);
                if (MD5_BLOCKSIZE + len > sizeof(right)) {
                    log_error("smtp", "right overflow");
                    SMTP_ABORT(&smtp_state.conn)
                }

                os_memcpy(right, key, MD5_BLOCKSIZE);
                for (i=0; i<MD5_BLOCKSIZE; i++)
                    right[i] ^= 0x36;
                os_memcpy(right+MD5_BLOCKSIZE, chal, len);

                right[MD5_BLOCKSIZE + len] = 0;
                log_debug("smtp", "right = %02x%02x%02x%02x%02x%02x%02x%02x"
                                          "%02x%02x%02x%02x%02x%02x%02x%02x%s",
                                  right[0],right[1],right[2], right[3], right[4], right[5], right[6], right[7],
                                  right[8],right[9],right[10],right[11],right[12],right[13],right[14],right[15],
                                  right+16);

                /* Reusing right as a tmp buffer */

                md5(right, MD5_BLOCKSIZE + len, right+MD5_BLOCKSIZE);
                log_debug("smtp", "md5(right) = %02x%02x%02x%02x%02x%02x%02x%02x"
                                               "%02x%02x%02x%02x%02x%02x%02x%02x",
                                  right[64+0],right[64+1],right[64+2], right[64+3], right[64+4], right[64+5], right[64+6], right[64+7],
                                  right[64+8],right[64+9],right[64+10],right[64+11],right[64+12],right[64+13],right[64+14],right[64+15]);

                os_memcpy(right, left, MD5_BLOCKSIZE);
                md5(right, MD5_BLOCKSIZE + MD5_HASHLEN, resp);
                log_debug("smtp", "resp = %02x%02x%02x%02x%02x%02x%02x%02x"
                                         "%02x%02x%02x%02x%02x%02x%02x%02x",
                                  resp[0],resp[1],resp[2], resp[3], resp[4], resp[5], resp[6], resp[7],
                                  resp[8],resp[9],resp[10],resp[11],resp[12],resp[13],resp[14],resp[15]);

                len = os_strlen(SMTP_USER) + 33;
                if (len > sizeof(right)) {
                    log_error("smtp", "right overflow");
                    SMTP_ABORT(&smtp_state.conn)
                }
                os_snprintf((char *)right, sizeof(right),
                            "%s %02x%02x%02x%02x%02x%02x%02x%02x"
                               "%02x%02x%02x%02x%02x%02x%02x%02x",
                            SMTP_USER,
                            resp[0],resp[1],resp[2], resp[3], resp[4], resp[5], resp[6], resp[7],
                            resp[8],resp[9],resp[10],resp[11],resp[12],resp[13],resp[14],resp[15]);

                log_debug("smtp", "%s", right);

                len2 = base64_encode(right, len, smtp_state.outbuf,
                                     sizeof(smtp_state.outbuf));
                if (len2 < 0)
                    SMTP_ABORT(&smtp_state.conn)

                os_memcpy(smtp_state.outbuf+len2, "\r\n", 2);
                len2 += 2;
            }

            if (espconn_send(&smtp_state.conn, smtp_state.outbuf, len2)) {
                log_error("smtp", "espconn_send() failed");
                SMTP_ABORT(&smtp_state.conn)
            }

            smtp_state.outbuf[len2-2] = 0;
            log_debug("smtp", ">> %s", smtp_state.outbuf);

            smtp_state.state = SMTP_STATE_CHAL_RESP;
        }
    }

    else if (smtp_state.state == SMTP_STATE_CHAL_RESP) {
        uint8_t *end;

        smtp_state.inbuf[smtp_state.inbufused] = 0;

        end = (uint8_t *)os_strstr((char *)smtp_state.inbuf, "\r\n");
        if (end != NULL) {
            *end = 0;
            log_debug("smtp", "<< %s", smtp_state.inbuf);
            *end = '\r';

            if (os_strncmp("235 ", (char *)smtp_state.inbuf, 4) != 0) {
                log_error("smtp", "non 235 response");
                SMTP_ABORT(&smtp_state.conn)
            }

            end += 2;
            smtp_state.inbufused -= end - smtp_state.inbuf;
            os_memmove(smtp_state.inbuf, end, smtp_state.inbufused+1);

            os_snprintf((char *)smtp_state.outbuf, sizeof(smtp_state.outbuf),
                        "mail FROM:<%s>\r\n", SMTP_FROM);

            if (espconn_send(&smtp_state.conn, smtp_state.outbuf,
                             os_strlen(SMTP_FROM) + 14)) {
                log_error("smtp", "espconn_send() failed");
                SMTP_ABORT(&smtp_state.conn)
            }

            smtp_state.outbuf[os_strlen(SMTP_FROM) + 12] = 0;
            log_debug("smtp", ">> %s", smtp_state.outbuf);

            smtp_state.state = SMTP_STATE_FROM_RESP;
        }
    }

    else if (smtp_state.state == SMTP_STATE_FROM_RESP) {
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
        SMTP_ABORT(conn)
    }

    if (espconn_regist_recvcb(conn, smtp_recv_cb)) {
        log_error("smtp", "espconn_regist_recvcb() failed");
        SMTP_ABORT(conn)
    }

    if (espconn_regist_sentcb(conn, smtp_sent_cb)) {
        log_error("smtp", "espconn_regist_sentcb() failed");
        SMTP_ABORT(conn)
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
        SMTP_ABORT(conn)
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
