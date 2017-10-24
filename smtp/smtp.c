#include "../config.h"

#include <user_interface.h>

struct {
    #define SMTP_STATE_READY 0
    int state;
} smtp_state;

ICACHE_FLASH_ATTR void smtp_send(const char *to, const char *subj,
                                 const char *body) {
    smtp_state.state = SMTP_STATE_READY;

    /* FIXME STOPPED */

}
