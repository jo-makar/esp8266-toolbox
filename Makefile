TOOLCHAIN_BASEPATH = /home/joe/local/local-esp-open-sdk/xtensa-lx106-elf
SDK_BASEPATH       = $(TOOLCHAIN_BASEPATH)/xtensa-lx106-elf/sysroot/usr

########################################

src = $(wildcard *.c) $(wildcard httpd/*.c)
obj = $(src:.c=.o)
dep = $(src:.c=.d)

app_elf = app.elf
addr1   = 0x00000
addr2   = 0x40000

app_bin1 = $(app_elf)-$(addr1).bin
app_bin2 = $(app_elf)-$(addr2).bin

# The --start,end-group is necessary for the references between the SDK libraries
sdklibs = --start-group -llwip -lmain -lnet80211 -lphy -lpp -lwpa --end-group
LDLIBS  = $(TOOLCHAIN_BASEPATH)/lib/gcc/xtensa-lx106-elf/4.8.2/libgcc.a

CC = $(TOOLCHAIN_BASEPATH)/bin/xtensa-lx106-elf-cc
LD = $(TOOLCHAIN_BASEPATH)/bin/xtensa-lx106-elf-ld

CFLAGS  = -Os -Wall -Wextra -fno-inline-functions -mlongcalls -mtext-section-literals -DICACHE_FLASH
LDFLAGS = -EL -nostdlib -static

ESPTOOL = $(TOOLCHAIN_BASEPATH)/bin/esptool.py

$(app_bin1) $(app_bin2): $(app_elf)
	$(ESPTOOL) elf2image $<

$(app_elf): $(obj)
	$(LD) $(LDFLAGS) -L$(SDK_BASEPATH)/lib -T eagle.app.v6.ld -o $@ $^ $(sdklibs) $(LDLIBS)

%.o: %.c
	$(CC) $(CFLAGS) -I. -I$(SDK_BASEPATH)/include -MD -MF $(@:.o=.d) -c -o $@ $<

-include $(dep)

clean:
	rm -f $(app_bin1) $(app_bin2) $(app_elf) $(obj) $(dep)

# Keep GPIO0 low during power on to put the ESP8266 in bootloader mode (typically by holding a button down on breakouts)
flash: $(app_bin1) $(app_bin2)
	$(ESPTOOL) write_flash $(addr1) $(app_bin1) $(addr2) $(app_bin2)
