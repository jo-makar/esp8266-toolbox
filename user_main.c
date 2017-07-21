#include <ip_addr.h> /* Must be included before espconn.h */

#include <esp_sdk_ver.h>
#include <espconn.h>

#include "httpd/httpd.h"
#include "config.h"
#include "log.h"
#include "wifi.h"

#if ESP_SDK_VERSION < 020000
    #error "ESP8266 SDK version >= 2.0.0 required"
#endif

void user_init() {
    wifi_init();

    /* TODO Increment as clients are written; eg for FOTA, NTP, MQTT, SMTP */
    if (espconn_tcp_set_max_con(MAX_CONN_INBOUND))
        CRITICAL("espconn_tcp_set_max_con() failed\n")

    httpd_init();
}
