/*
 * Hardware:  ESP-12-E, 512 kbyte flash
 * Toolchain: https://github.com/pfalcon/esp-open-sdk
 *
 * Debug output made available on UART0 at 74880 baud, 8N1
 */

#include "httpd/httpd_init.h"
#include "mqtt/mqtt_init.h"
#include "wifi.h"

#include <c_types.h>

void user_init() {
    /*
     * Servers initialize and begin accepting here, clients initialize here and connect
     * on wifi station connect events (which provides a link to the outer network).
     */

    httpd_init();
    mqtt_init();

    wifi_init();
}
