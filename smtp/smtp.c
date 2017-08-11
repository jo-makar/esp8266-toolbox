#include "../log/log.h"
#include "../missing-decls.h"
#include "../param.h"
#include "private.h"

#include <osapi.h>

SmtpServer smtp_server;

ICACHE_FLASH_ATTR void smtp_init() {
    os_bzero(&smtp_server, sizeof(smtp_server));
    if (param_retrieve(PARAM_SMTP_OFFSET, (uint8_t *)&smtp_server, sizeof(smtp_server)))
        return;

    DEBUG(SMTP, "host:port=%s:%u user=%s", smtp_server.host, smtp_server.port, smtp_server.user)
}

ICACHE_FLASH_ATTR void smtp_send(const char *from, const char *to,
                                 const char *subj, const char *body) {
    /* FIXME STOPPED */
}

ICACHE_FLASH_ATTR void smtp_send_cb(const char *from, const char *to,
                                    const char *subj, int (body_cb)(void *)) {
    /* FIXME Needed for smtp_send_status() that will send the logs */
}
