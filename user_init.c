#include "log/log.h"
#include "http/http.h"
#include "missing-decls.h"
#include "uptime.h"
#include "wifi.h"

#include <esp_sdk_ver.h>
#include <ets_sys.h>
#include <osapi.h>

#if ESP_SDK_VERSION < 020000
    #error "ESP8266 SDK version >= 2.0.0 required"
#endif

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
}
