#include <ip_addr.h> /* Must be included before espconn.h */

#include <esp_sdk_ver.h>
#include <espconn.h>

#include "httpd/httpd.h"
#include "smtp/smtp.h"
#include "config.h"
#include "log.h"
#include "uptime.h"
#include "wifi.h"

#if ESP_SDK_VERSION < 020000
    #error "ESP8266 SDK version >= 2.0.0 required"
#endif

ICACHE_FLASH_ATTR void user_init() {
    log_init();

    LOG_INFO(MAIN, "Version %u.%u.%u built on %s\n",
             VERSION_MAJOR, VERSION_MINOR, VERSION_PATCH, BUILD_DATETIME)

    /*
     * The SDK function system_get_time() overflows about every 71 mins.
     * This timer ensure that overflow is handled cleanly,
     * and supports uptime_us() which has an overflow of >500k years.
     */
    os_timer_disarm(&uptime_overflow_timer);
    os_timer_setfn(&uptime_overflow_timer, uptime_overflow_handler, NULL);
    os_timer_arm(&uptime_overflow_timer, 1000*60*5, true);

    wifi_init();

    if (!espconn_secure_set_size(1, 8*1024))
        LOG_ERROR(SMTP, "espconn_secure_set_size() failed\n")
    if (!espconn_secure_set_default_certificate(httpd_cert, httpd_cert_len))
        LOG_ERROR(SMTP, "espconn_secure_set_default_certificate() failed\n")
    if (!espconn_secure_set_default_private_key(httpd_key_der,
                                                httpd_key_der_len))
        LOG_ERROR(SMTP, "espconn_secure_set_default_private_key() failed\n")

    httpd_init();
}

/*
 * Callback for after the system has been fully initialized, including an
 * established connection to an AP (for outgoing connections).
 */
ICACHE_FLASH_ATTR void user_init_net() {
    char subj[256];
    static uint8_t init = 0;

    if (init == 0) {
        /* FIXME STOPPED
        os_snprintf(subj, sizeof(subj), "Version %u.%u.%u online");
        smtp_send("jomakar.app@gmail.com", subj, "");
        */
        init = 1;
    }
}
