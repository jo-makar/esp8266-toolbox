include config.mk

SRC = $(wildcard *.c)
OBJ = $(SRC:.c=.o)
DEP = $(SRC:.c=.d)

CC = $(TOOLCHAIN_PATH)/xtensa-lx106-elf-gcc
LD = $(TOOLCHAIN_PATH)/xtensa-lx106-elf-ld

OBJCOPY = $(TOOLCHAIN_PATH)/xtensa-lx106-elf-objcopy
OBJDUMP = $(TOOLCHAIN_PATH)/xtensa-lx106-elf-objdump

GEN_APPBIN = $(SDK_PATH)/tools/gen_appbin.py

CFLAGS = -Os -Wall -Wextra -DICACHE_FLASH -I$(SDK_PATH)/include

LDFLAGS = -L$(SDK_PATH)/ld -L$(SDK_PATH)/lib \
          -L$(TOOLCHAIN_PATH)/../xtensa-lx106-elf/sysroot/usr/lib \
          -L$(TOOLCHAIN_PATH)/../xtensa-lx106-elf/sysroot/lib

SDK_LIBS = --start-group -lc -lgcc -lhal -llwip -lmain -lnet80211 -lphy -lpp -lwpa --end-group

# The first partition is half the flash minus 4KB (boot) and 8KB (OTA key).
# The second partition is half the flash minus 4KB (blank) and 16KB (sys data).
# Hence the max size of an app is the space available in the second partition.
MAX_APP_SIZE = $(shell echo '($(FLASH_SIZE_KB)/2 - (4+16)) * 1024' | bc)

all: app1.bin app2.bin

app1.bin: app1.elf
	@echo GEN_APPBIN $<

	@$(OBJCOPY) -j      .text -O binary $< eagle.app.v6.text.bin
	@$(OBJCOPY) -j.irom0.text -O binary $< eagle.app.v6.irom0text.bin
	@$(OBJCOPY) -j      .data -O binary $< eagle.app.v6.data.bin
	@$(OBJCOPY) -j    .rodata -O binary $< eagle.app.v6.rodata.bin

	@rm -f eagle.app.flash.bin
	@COMPILE=gcc PATH=$$PATH:$(TOOLCHAIN_PATH) python $(GEN_APPBIN) $< 2 0 15 $(FLASH_SIZE_GEN_APPBIN) 0 >/dev/null
	@rm eagle.app.v6.*.bin
	@mv eagle.app.flash.bin $@

	@test `du -b $@ | awk '{print $$1}'` -le $(MAX_APP_SIZE) || false

app2.bin: app2.elf
	@echo GEN_APPBIN $<

	@$(OBJCOPY) -j      .text -O binary $< eagle.app.v6.text.bin
	@$(OBJCOPY) -j.irom0.text -O binary $< eagle.app.v6.irom0text.bin
	@$(OBJCOPY) -j      .data -O binary $< eagle.app.v6.data.bin
	@$(OBJCOPY) -j    .rodata -O binary $< eagle.app.v6.rodata.bin

	@rm -f eagle.app.flash.bin
	@COMPILE=gcc PATH=$$PATH:$(TOOLCHAIN_PATH) python $(GEN_APPBIN) $< 2 0 15 $(FLASH_SIZE_GEN_APPBIN) 0 >/dev/null
	@rm eagle.app.v6.*.bin
	@mv eagle.app.flash.bin $@

	@test `du -b $@ | awk '{print $$1}'` -le $(MAX_APP_SIZE) || false

	@# Display various application statistics

	@   datalen=`$(OBJDUMP) -h -j      .data $< | awk '      /.data/ {print strtonum("0x"$$3)}'`; \
	  rodatalen=`$(OBJDUMP) -h -j    .rodata $< | awk '    /.rodata/ {print strtonum("0x"$$3)}'`; \
	     bsslen=`$(OBJDUMP) -h -j       .bss $< | awk '       /.bss/ {print strtonum("0x"$$3)}'`; \
	    textlen=`$(OBJDUMP) -h -j      .text $< | awk '      /.text/ {print strtonum("0x"$$3)}'`; \
	 romtextlen=`$(OBJDUMP) -h -j.irom0.text $< | awk '/.irom0.text/ {print strtonum("0x"$$3)}'`; \
         \
         echo; \
         echo "      .data = $$datalen"; \
         echo "    .rodata = $$rodatalen"; \
         echo "       .bss = $$bsslen"; \
         echo "      .text = $$textlen"; \
         echo ".irom0.text = $$romtextlen"; \
         echo; \
         echo '  RAM:' `echo "scale = 2; ($$datalen + $$bsslen + $$textlen) / 1024" | bc` KB; \

	@du -b $@ | awk '{printf "Flash: %.2f KB, %.2f%%\n", $$1/1024, $$1/$(MAX_APP_SIZE)*100}'; echo

app1.elf: $(OBJ)
	@echo LD $@
	@$(LD) $(LDFLAGS) -Teagle.app.v6.new.1024.app1.ld -o $@ $^ $(SDK_LIBS)

app2.elf: $(OBJ)
	@echo LD $@
	@$(LD) $(LDFLAGS) -Teagle.app.v6.new.1024.app2.ld -o $@ $^ $(SDK_LIBS)

%.o: %.c
	@echo CC $@
	@$(CC) $(CFLAGS) -MD -MF $(@:.o=.d) -c -o $@ $<

-include $(DEP)

clean:
	@rm -f app?.bin eagle.app.*.bin app?.elf $(OBJ) $(DEP)
