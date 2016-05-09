TOOLCHAIN_BASEPATH = /home/joe/local/local-esp-open-sdk/xtensa-lx106-elf
SDK_BASEPATH       = $(TOOLCHAIN_BASEPATH)/xtensa-lx106-elf/sysroot/usr

########################################

src = $(wildcard *.c) $(wildcard httpd/*.c)
obj = $(src:.c=.o)
dep = $(src:.c=.d)

# The --start,end-group is necessary for the references between the SDK libraries
sdklibs = --start-group -llwip -lmain -lnet80211 -lphy -lpp -lwpa --end-group
LDLIBS  = $(TOOLCHAIN_BASEPATH)/lib/gcc/xtensa-lx106-elf/4.8.2/libgcc.a

CC = $(TOOLCHAIN_BASEPATH)/bin/xtensa-lx106-elf-cc
LD = $(TOOLCHAIN_BASEPATH)/bin/xtensa-lx106-elf-ld

OBJDUMP = $(TOOLCHAIN_BASEPATH)/bin/xtensa-lx106-elf-objdump
OBJCOPY = $(TOOLCHAIN_BASEPATH)/bin/xtensa-lx106-elf-objcopy

# When the macro ICACHE_FLASH is defined code/rodata marked with ICACHE_FLASH/RODATA_ATTR gets loaded into
# the irom0text.bin (irom0_0_seg) section of flash and are loaded on demand into the MCUs RAM for execution.
# Otherwise code/rodata is loaded into the significantly smaller flash.bin (iram1_0_seg).
# NB Some code, such as ISRs, must not be marked with with ICACHE_FLASH_ATTR.

CFLAGS  = -Os -Wall -Wextra -fno-inline-functions -ffunction-sections -fdata-sections -mlongcalls -mtext-section-literals -DICACHE_FLASH
LDFLAGS = -EL -nostdlib -static --gc-sections

ESPTOOL = $(TOOLCHAIN_BASEPATH)/bin/esptool.py

user1.bin user2.bin: app1.elf app2.elf
	@# NB $(OBJDUMP) -h app{1,2}.elf can be used to list section headers

	$(OBJCOPY) -j .text       -O binary app1.elf eagle.app.v6.text.bin
	$(OBJCOPY) -j .irom0.text -O binary app1.elf eagle.app.v6.irom0text.bin
	$(OBJCOPY) -j .data       -O binary app1.elf eagle.app.v6.data.bin
	$(OBJCOPY) -j .rodata     -O binary app1.elf eagle.app.v6.rodata.bin
	@# boot_mode = 2, flash_mode = 0/QIO,  flash_clk_div = 15 (80 Mhz), flash_size_map = 2 (1024 KB), user_bin = 0
	COMPILE=gcc PATH=$$PATH:`dirname $(CC)` python gen_appbin.py app1.elf 2 0 15 2 0
	mv eagle.app.flash.bin user1.bin
	rm -f eagle.app.v6.*.bin

	$(OBJCOPY) -j .text       -O binary app2.elf eagle.app.v6.text.bin
	$(OBJCOPY) -j .irom0.text -O binary app2.elf eagle.app.v6.irom0text.bin
	$(OBJCOPY) -j .data       -O binary app2.elf eagle.app.v6.data.bin
	$(OBJCOPY) -j .rodata     -O binary app2.elf eagle.app.v6.rodata.bin
	COMPILE=gcc PATH=$$PATH:`dirname $(CC)` python gen_appbin.py app2.elf 2 0 15 2 0
	mv eagle.app.flash.bin user2.bin
	rm -f eagle.app.v6.*.bin

app1.elf app2.elf: $(obj)
	$(LD) $(LDFLAGS) -L$(SDK_BASEPATH)/lib -T ld/eagle.app.v6.new.1024.app1.ld -o app1.elf $^ $(sdklibs) $(LDLIBS)
	$(LD) $(LDFLAGS) -L$(SDK_BASEPATH)/lib -T ld/eagle.app.v6.new.1024.app2.ld -o app2.elf $^ $(sdklibs) $(LDLIBS)

%.o: %.c
	$(CC) $(CFLAGS) -I. -I$(SDK_BASEPATH)/include -MD -MF $(@:.o=.d) -c -o $@ $<

-include $(dep)

clean:
	rm -f user1.bin user2.bin app1.elf app2.elf $(obj) $(dep)

# Flashing over USB requires that the ESP8266 be in bootloader mode, this can be done by holding GPIO0 low during boot

flash: user1.bin
	$(ESPTOOL) write_flash -fs 8m -ff 80m 0x00000 boot_v1.5.bin 0x01000 user1.bin \
		0x7e000 blank.bin 0xfc000 esp_init_data_default.bin 0xfe000 blank.bin

flash-user1: user1.bin
	$(ESPTOOL) write_flash -fs 8m -ff 80m 0x01000 user1.bin
