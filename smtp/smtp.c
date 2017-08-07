#include <osapi.h>

#include "../log.h"
#include "smtp.h"

Smtp smtp;

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

    /*
     * FIXME STOPPED
     * espconn_regist_connectcb, reconcb (secure versions?)
     * espconn_secure_connect
     * in connect callback: espconn_regist_recvcb, sentcb, disconcb
     */

    return 0;
}
