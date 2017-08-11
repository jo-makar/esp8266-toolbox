TOOLCHAIN_PATH = $(HOME)/local/esp-open-sdk/xtensa-lx106-elf/bin

SDK_PATH = $(HOME)/local/esp-open-sdk/ESP8266_NONOS_SDK_V2.0.0_16_08_10

FLASH_SIZE_KB = 1024
ifeq ($(FLASH_SIZE_KB), 1024)
    FLASH_SIZE_GEN_APPBIN = 2
    FLASH_SIZE_ESPTOOL    = 8m
else
    $(error Untested FLASH_SIZE_KB)
endif

UART0_PORT = /dev/ttyUSB0
