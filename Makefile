# Build an application without OTA upgrade support

TOOLCHAIN_PATH ?= $(HOME)/local/esp-open-sdk/xtensa-lx106-elf/bin
SDK_PATH ?= $(HOME)/local/esp-open-sdk/xtensa-lx106-elf/xtensa-lx106-elf/sysroot

PORT ?= /dev/ttyUSB0

#################################################################################

SRC = $(wildcard *.c)
OBJ = $(SRC:.c=.o)
DEP = $(SRC:.c=.d)

CC = $(TOOLCHAIN_PATH)/xtensa-lx106-elf-gcc
LD = $(TOOLCHAIN_PATH)/xtensa-lx106-elf-ld

ESPTOOL = $(TOOLCHAIN_PATH)/esptool.py

# When ICACHE_FLASH is defined, code (rodata) marked with ICACHE_FLASH_ATTR
# (ICACHE_RODATA_ATTR) gets loaded into the irom0text section rather than RAM.
# NB Some code, such as ISRs, must not be marked with the ICACHE_FLASH_ATTR.

CFLAGS = -Os -Wall -Wextra -mlongcalls -mtext-section-literals -DICACHE_FLASH
LDFLAGS = -EL -nostdlib -static --gc-sections

# The --start/end-group is necessary for the references between the libraries
SDKLIBS = --start-group -lc -lgcc -lhal -llwip -lmain -lnet80211 -lpp -lphy \
                        -lwpa --end-group

app.elf-0x00000.bin app.elf-0x40000.bin: app.elf
	@echo ELF2IMAGE $<
	@$(ESPTOOL) elf2image $<

	@# The _irom0_text_start is set incorrectly in the linker script
	@# which makes the app.elf-0x10000.bin filename incorrect
	@mv app.elf-0x10000.bin app.elf-0x40000.bin

	@# Verify app.elf-0x00000.bin (user app) is <= 248K
	@du -b app.elf-0x00000.bin
	@test `du -b app.elf-0x00000.bin | awk '{print $$1}'` -le 253952 || false

	@# Verify app.elf-0x40000.bin (sdk libs) is <= 240K
	@du -b app.elf-0x40000.bin
	@test `du -b app.elf-0x40000.bin | awk '{print $$1}'` -le 245760  || false

app.elf: $(OBJ)
	@echo LD $@ ...
	@$(LD) $(LDFLAGS) -L$(SDK_PATH)/usr/lib -L$(SDK_PATH)/lib \
		-T$(SDK_PATH)/usr/lib/eagle.app.v6.ld -o $@ $^ $(SDKLIBS)

%.o: %.c
	@echo CC $<
	@$(CC) $(CFLAGS) -MD -MF $(@:.o=.d) -I. -c $<

-include $(DEP)

clean:
	@rm -f app.elf app.elf-*.bin $(OBJ) $(DEP)

# Hold GPIO0 low during boot to flash over USB
flash: app.elf-0x00000.bin app.elf-0x40000.bin
	@echo WRITE_FLASH app.elf-\*.bin
	@$(ESPTOOL) -p $(PORT) write_flash 0x00000 app.elf-0x00000.bin 0x40000 app.elf-0x40000.bin
