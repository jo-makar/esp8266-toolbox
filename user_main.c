#include <ip_addr.h> /* Must be included before espconn.h */

#include <esp_sdk_ver.h>
#include <espconn.h>

#include "httpd/httpd.h"
#include "config.h"
#include "log.h"
#include "uptime.h"
#include "wifi.h"

#if ESP_SDK_VERSION < 020000
    #error "ESP8266 SDK version >= 2.0.0 required"
#endif

ICACHE_FLASH_ATTR void user_init() {
    /* FIXME Remove */
    {
        #include "crypto/bigint.h"

        Bigint p, a, b;

        bigint_fromhex(&a, "2b");
        bigint_fromhex(&b, "0a");
        bigint_mult(&p, &a, &b);

        os_printf("p = "); bigint_print(&p); os_printf("\n");
    }

    /*
     * The SDK function system_get_time() overflows about every 71 mins.
     * This timer ensure that overflow is handled cleanly,
     * and supports uptime_us() which has an overflow of >500k years.
     */
    os_timer_disarm(&uptime_overflow_timer);
    os_timer_setfn(&uptime_overflow_timer, uptime_overflow_handler, NULL);
    os_timer_arm(&uptime_overflow_timer, 1000*60*5, true);

    wifi_init();

    if (espconn_tcp_set_max_con(MAX_CONN))
        CRITICAL(MAIN, "espconn_tcp_set_max_con() failed\n")

    httpd_init();
}
