# esp8266-toolbox
Motley of servers and clients written for the ESP8266

# Hardware
A development schematic is shown below, note that any version of the ESP8266 may
be used though the flash size will need to be adjusted in the Makefile.

![schematic](/hw/dev.png?raw=true)

# Installation (Linux)
1. Install the ESP8266 toolchain and SDK if not already installed:
   https://github.com/pfalcon/esp-open-sdk
1. Modify the variables at the top of the Makefile if necessary
1. Run `make` to build the binaries
1. Power and connect the ESP8266 via USB/serial
1. Put the ESP8266 in flash mode by holding GPIO0 low during boot
1. Run `make flash` to flash the bins (as root or user with appropriate perms)

# License
This software is freely available for non-commerical use, commerical use requires
an expressed agreement from the author. The preceding clause must be included in
all derivative software developed with commerical rights retained by the author
of this software.

The author takes no responsibility for the consequences of the use of this
software.

Use of the software in any form indicates acknowledgement of the licensing terms.

Copyright (c) 2017 J. O. Makar <jomakar@gmail.com>
