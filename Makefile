# Build an application with OTA upgrade support

# Written against the Adafruit Huzzah ESP8266 breakout
# available from https://www.adafruit.com/product/2471
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

# The first partition is half the flash minus 4KB (boot) and 8KB (OTA key).
# The second partition is half the flash minus 4KB (blank) and 16KB (sys data).
# Hence the max size of an app is the space available in the second partition.
MAX_APP_SIZE = $(shell echo '($(FLASH_SIZE_KB)/2 - (4+16)) * 1024' | bc)

PORT ?= /dev/ttyUSB0

#################################################################################

SRC = $(wildcard *.c drivers/*.c)
OBJ = $(SRC:.c=.o)
DEP = $(SRC:.c=.d)

CC = $(TOOLCHAIN_PATH)/xtensa-lx106-elf-gcc
LD = $(TOOLCHAIN_PATH)/xtensa-lx106-elf-ld

OBJCOPY = $(TOOLCHAIN_PATH)/xtensa-lx106-elf-objcopy

ESPTOOL = $(TOOLCHAIN_PATH)/esptool.py

GEN_APPBIN = $(NONOS_SDK_PATH)/tools/gen_appbin.py

CFLAGS = -Os -Wall -Wextra -mlongcalls -mtext-section-literals
LDFLAGS = -EL -nostdlib -static --gc-sections

# The --start/end-group is necessary for the references between the libraries
SDKLIBS = --start-group -lc -lgcc -lhal -llwip -lmain -lnet80211 -lpp -lphy \
              -lwpa --end-group

ESP_DATA_BIN = $(NONOS_SDK_PATH)/bin/esp_init_data_default.bin
BLANK_BIN = $(NONOS_SDK_PATH)/bin/blank.bin

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
	@rm -f eagle.app.flash.bin

	@COMPILE=gcc PATH=$$PATH:$(TOOLCHAIN_PATH) python $(GEN_APPBIN) $< \
             2 0 15 $(FLASH_SIZE_GEN_APPBIN) 0

	@rm eagle.app.v6.*.bin

	@# Verify eagle.app.flash.bin (user app) is within size limits
	@du -b eagle.app.flash.bin
	@test `du -b eagle.app.flash.bin | awk '{print $$1}'` \
         -le $(MAX_APP_SIZE) || false

app.elf: $(OBJ)
	@echo LD $@ ...
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
	@echo WRITE_FLASH app.elf-\*.bin
	@$(ESPTOOL) -p $(PORT) write_flash \
             -ff 80m -fm qio -fs $(FLASH_SIZE_ESPTOOL) --verify \
             0x00000 $(NONOS_SDK_PATH)/bin/boot_v1.6.bin \
             0x01000 eagle.app.flash.bin \
             0x7e000 $(NONOS_SDK_PATH)/bin/blank.bin \
             0xfc000 $(NONOS_SDK_PATH)/bin/esp_init_data_default.bin \
             0xfe000 $(NONOS_SDK_PATH)/bin/blank.bin
