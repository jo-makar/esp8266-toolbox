#include <esp_sdk_ver.h>

#include "drivers/uart.h"

#include "wifi.h"

#if ESP_SDK_VERSION < 020000
    #error "ESP8266 SDK version >= 2.0.0 required"
#endif

void user_init() {
    /* Leave the bit rates unchanged but ensure UART1 is initialized */
    uart_init(BIT_RATE_74880, BIT_RATE_74880);
    /* Send os_printf() debug output to UART1 */
    /* TODO UART_SetPrintPort(UART1); */

    wifi_init();
}
