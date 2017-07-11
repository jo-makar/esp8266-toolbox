#include <c_types.h>
#include <osapi.h>
#include <user_interface.h>

#include "missing-decls.h"
#include "drivers/uart.h"

ICACHE_FLASH_ATTR void user_init() {
    /* Leave the bit rates unchanged but ensure UART1 is initialized */
    uart_init(BIT_RATE_74880, BIT_RATE_74880);
    /* Send os_printf() debug output to UART1 */
    /* TODO UART_SetPrintPort(UART1); */

    os_printf("SDK version: %s\n", system_get_sdk_version());
    system_print_meminfo();
    os_printf("system_get_flash_size_map = %d\n", system_get_flash_size_map());
}
