#include "config.h"
#include "log.h"
#include "wifi.h"

#include <esp_sdk_ver.h>
#include <user_interface.h>

#if ESP_SDK_VERSION < 020000
    #error "ESP8266 SDK version >= 2.0.0 required"
#endif

ICACHE_FLASH_ATTR void user_init() {
    log_info("main", "Version %s built on %s", VERSION, BUILD_DATETIME);

    wifi_init();
}
