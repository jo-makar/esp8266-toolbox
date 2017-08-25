#include "drivers/tsl2561.h"
#include "i2c/i2c_master.h"
#include "log/log.h"
#include "http/http.h"
#include "smtp/smtp.h"
#include "missing-decls.h"
#include "uptime.h"
#include "wifi.h"

#include <esp_sdk_ver.h>
#include <ets_sys.h>
#include <osapi.h>
#include <stdlib.h>

#if ESP_SDK_VERSION < 020000
    #error "ESP8266 SDK version >= 2.0.0 required"
#endif

os_timer_t monitor_timer;
static void monitor(void *arg);

ICACHE_FLASH_ATTR void user_init() {
    log_init();

    INFO(MAIN, "Version %s built on %s", VERSION, BUILD_DATETIME)

    /*
     * The SDK function system_get_time() overflows about every 71 mins.
     * This timer ensure that overflow is handled cleanly,
     * and supports uptime_us() which has an overflow of >500k years.
     */
    os_timer_disarm(&uptime_timer);
    os_timer_setfn(&uptime_timer, uptime_handler, NULL);
    os_timer_arm(&uptime_timer, 1000*60*20, true);

    wifi_init();
    http_init();
    smtp_init();

    /* Initialize I2C on GPIO0 (SDA) and GPIO2 (SCL) */
    i2c_master_gpio_init();

    os_timer_disarm(&monitor_timer);
    os_timer_setfn(&monitor_timer, monitor, NULL);
    os_timer_arm(&monitor_timer, 1000*30, true);
}

ICACHE_FLASH_ATTR void monitor(void *arg) {
    (void)arg;

    static int32_t oldlux = -1;
    static uint64_t lastmail = 0;
    int32_t lux;
    uint64_t now;
    char subj[64];

    if ((lux = tsl2561_lux()) != -1) {
        DEBUG(MAIN, "Light = %d lx", lux)
        if (oldlux != -1) {
            if (lux > oldlux*14/10) {
                now = uptime_us();
                if (llabs(now - lastmail) / 1000000 > 5*60) {
                    os_snprintf(subj, sizeof(subj),
                                "esp8266-%08x sharp lux increase",
                                system_get_chip_id());
                    smtp_send(smtp_server.from, smtp_server.to, subj, "");
                    lastmail = now;
                }
            }
        }
        oldlux = lux;
    }
}
