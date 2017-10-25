#include "../config.h"
#include "../log.h"
#include "../missing-decls.h"
#include "smtp.h"

#include <osapi.h>
#include <user_interface.h>

struct {
    #define SMTP_STATE_ERROR -1
    #define SMTP_STATE_READY  0
    int8_t state;

    char to[128];
    char subj[128];
    char body[SMTP_STATE_BODYLEN];

    os_timer_t timer;
} smtp_state;

ICACHE_FLASH_ATTR char *smtp_send_bodybuf() {
    return smtp_state.body;
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

    /* FIXME STOPPED */
}

ICACHE_FLASH_ATTR int8_t smtp_send_status() {
    return smtp_state.state;
}

ICACHE_FLASH_ATTR void smtp_send_reset() {
    if (smtp_state.state == SMTP_STATE_ERROR)
        smtp_state.state = SMTP_STATE_READY;
}
