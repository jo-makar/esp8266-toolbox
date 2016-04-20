/*
 * Hardware:  ESP-12-E, 512 kbyte flash
 * Toolchain: https://github.com/pfalcon/esp-open-sdk
 *
 * Debug output made available on UART0 at 74880 baud, 8N1
 */

#include "httpd/httpd_init.h"
#include "wifi.h"

void user_init() {
    wifi_init();
    httpd_init();
}
