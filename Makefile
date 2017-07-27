# Build an application with OTA upgrade support

# Written against an ESP-1 board design
# (for example http://www.sparkfun.com/products/13678)
# and which has a flash of 8 Mbits (1 MB = 1024 KB)

# NB Flash size can be determined from system_get_flash_size_map()

TOOLCHAIN_PATH ?= $(HOME)/local/esp-open-sdk/xtensa-lx106-elf/bin
SDK_PATH ?= $(HOME)/local/esp-open-sdk/xtensa-lx106-elf/xtensa-lx106-elf/sysroot
NONOS_SDK_PATH ?= $(HOME)/local/esp-open-sdk/ESP8266_NONOS_SDK_V2.0.0_16_08_10

FLASH_SIZE_KB = 1024
# gen_appbin.py flash_size_map argument (2 => 1024 KB)
FLASH_SIZE_GEN_APPBIN ?= 2
# esptool.py flash_size argument (8m => 8 Mbits)
FLASH_SIZE_ESPTOOL ?= 8m

PORT ?= /dev/ttyUSB0

#################################################################################

SRC = $(wildcard *.c crypto/*.c httpd/*.c)
OBJ = $(SRC:.c=.o)
DEP = $(SRC:.c=.d)

CC = $(TOOLCHAIN_PATH)/xtensa-lx106-elf-gcc
LD = $(TOOLCHAIN_PATH)/xtensa-lx106-elf-ld

OBJCOPY = $(TOOLCHAIN_PATH)/xtensa-lx106-elf-objcopy

ESPTOOL = $(TOOLCHAIN_PATH)/esptool.py

GEN_APPBIN = $(NONOS_SDK_PATH)/tools/gen_appbin.py

# When the macro ICACHE_FLASH is defined code/rodata marked with
# ICACHE_{FLASH,RODATA}_ATTR gets loaded into the irom0_0_seg section of flash
# and are loaded on demand into the MCU's RAM for execution.
# Otherwise code/rodata is loaded into the significantly smaller iram1_0_seg.
# NB Some code, such as ISRs, must not be marked with with ICACHE_FLASH_ATTR.

CFLAGS = -Os -Wall -Wextra -mlongcalls -mtext-section-literals -DICACHE_FLASH
LDFLAGS = -EL -nostdlib -static --gc-sections

# The --start/end-group is necessary for the references between the libraries
SDKLIBS = --start-group -lc -lgcc -lhal -llwip -lmain -lnet80211 -lpp -lphy \
              -lwpa --end-group

BOOT_BIN = $(NONOS_SDK_PATH)/bin/boot_v1.6.bin
BLANK_BIN = $(NONOS_SDK_PATH)/bin/blank.bin
ESP_DATA_BIN = $(NONOS_SDK_PATH)/bin/esp_init_data_default.bin

# The first partition is half the flash minus 4KB (boot) and 8KB (OTA key).
# The second partition is half the flash minus 4KB (blank) and 16KB (sys data).
# Hence the max size of an app is the space available in the second partition.
MAX_APP_SIZE = $(shell echo '($(FLASH_SIZE_KB)/2 - (4+16)) * 1024' | bc)

BLANK_ADDR1 = $(shell printf "0x%05x\n" \
                    `echo '($(FLASH_SIZE_KB)/2 - 8) * 1024' | bc`)
BLANK_ADDR2 = $(shell printf "0x%05x\n" \
                    `echo '($(FLASH_SIZE_KB)/2 - 4) * 1024' | bc`)

ESPDATA_ADDR = $(shell printf "0x%05x\n" \
                    `echo '($(FLASH_SIZE_KB) - 16) * 1024' | bc`)
BLANK_ADDR3 = $(shell printf "0x%05x\n" \
                    `echo '($(FLASH_SIZE_KB) - 12) * 1024' | bc`)
BLANK_ADDR4 = $(shell printf "0x%05x\n" \
                    `echo '($(FLASH_SIZE_KB) - 8) * 1024' | bc`)
BLANK_ADDR5 = $(shell printf "0x%05x\n" \
                    `echo '($(FLASH_SIZE_KB) - 4) * 1024' | bc`)

eagle.app.flash.bin: app.elf
	@echo GEN_APPBIN $<

	@# The gen_appbin.py script expects specific filenames
	@$(OBJCOPY) -j .text -O binary $< eagle.app.v6.text.bin
	@$(OBJCOPY) -j .irom0.text -O binary $< eagle.app.v6.irom0text.bin
	@$(OBJCOPY) -j .data -O binary $< eagle.app.v6.data.bin
	@$(OBJCOPY) -j .rodata -O binary $< eagle.app.v6.rodata.bin

	@# gen_appbin.py boot_mode = 2, flash_mode = 0 (QIO),
	@#               flash_clk_div = 15 (80 MHz), flash_size_map, user_bin = 0
	@# TODO Specific information on the various parameters here

	@# gen_appbin.py appends to an existing file
	@rm -f $@

	@COMPILE=gcc PATH=$$PATH:$(TOOLCHAIN_PATH) python $(GEN_APPBIN) $< \
             2 0 15 $(FLASH_SIZE_GEN_APPBIN) 0

	@rm eagle.app.v6.*.bin

	@# Verify eagle.app.flash.bin (user app) is within size limits
	@test `du -b $@ | awk '{print $$1}'` -le $(MAX_APP_SIZE) || false
	@du -b $@ | \
         awk '{printf "%.2f KB, %.3f%%\n", $$1/1024, $$1/$(MAX_APP_SIZE)*100}'

app.elf: $(OBJ)
	@echo LD $@
	@$(LD) $(LDFLAGS) -L$(SDK_PATH)/usr/lib -L$(SDK_PATH)/lib \
             -L$(NONOS_SDK_PATH)/ld -Teagle.app.v6.new.1024.app1.ld \
             -o $@ $^ $(SDKLIBS)

%.o: %.c
	@echo CC $<
	@$(CC) $(CFLAGS) -MD -MF $(@:.o=.d) -I. -c -o $@ $<

-include $(DEP)

clean:
	@rm -f eagle.app.*.bin app.elf $(OBJ) $(DEP)

flash: eagle.app.flash.bin
	@echo WRITE_FLASH

	@# Verify the sizes of the bin files
	@test `du -b $(BOOT_BIN) | awk '{print $$1}'` -le 4096 || false
	@test `du -b $(BLANK_BIN) | awk '{print $$1}'` -le 4096 || false
	@test `du -b $(ESP_DATA_BIN) | awk '{print $$1}'` -le 4096 || false

	@$(ESPTOOL) -p $(PORT) write_flash \
             -ff 80m -fm qio -fs $(FLASH_SIZE_ESPTOOL) --verify \
             0x00000 $(BOOT_BIN) \
             0x01000 eagle.app.flash.bin \
             $(BLANK_ADDR1) $(BLANK_BIN) \
             $(BLANK_ADDR2) $(BLANK_BIN) \
             $(ESPDATA_ADDR) $(ESP_DATA_BIN) \
             $(BLANK_ADDR3) $(BLANK_BIN) \
             $(BLANK_ADDR4) $(BLANK_BIN) \
             $(BLANK_ADDR5) $(BLANK_BIN)

fota: eagle.app.flash.bin
	@echo HTTP_UPDATE
	@sha256sum $< | awk '{print $$1}'
	@curl --data-binary @$< http://192.168.4.1/fota/push; echo

keys:
	@echo OPENSSL GENRSA
	test ! -e privkey.pem
	@openssl genrsa -out privkey.pem 512
	@openssl rsa -in privkey.pem -out pubkey.pem -pubout
	@openssl rsa -in pubkey.pem -pubin -text -noout
	@# FIXME STOPPED crypto/rsapubkey.py privkey.pem >crypto/key.c
