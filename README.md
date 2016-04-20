# esp8266-toolbox
Motley of basic and portable servers and clients written for the ESP8266 ultimately for use on other platforms as well.

# Complete
* HTTP server

# TODO
* Implement OTA flashing via HTTP POST
* Verify HTTP server implementation follows the RFCs
** Is there a testing procedure that can be followed to do this?
* SMTP client
* RabbitMQ client
* Host a filesystem off an SD card

# Hardware
Developing against the ESP-12-E, 512 kbyte flash.  Others will work fine but the linker script will need to be modified.

# Installation (Linux)
0. Modify the TOOLCHAIN_BASEPATH variable in the Makefile appropriately
1. Install the ESP8266 GCC toolchain and SDK: http://www.esp8266.com/wiki/doku.php?id=toolchain
2. Run 'make' to build the binaries
3. Put the ESP8266 in bootloader mode (hold GPI0 low during reset, typically by a button on breakout boards)
4. Run 'make flash' to flash (as root or user with permissions on /dev/ttyUSB0)

The esp8266-serial (pyserial dependency) is a small utility to view the debugging output of the ESP8266 on /dev/ttyUSB0,
note that cannot use a standard utility like minicom because the ESP8266 bootloader uses a baud of 74880
(which is being maintained with the application debugging output for consistency).

# License
This software is freely available for non-commerical use, commerical use requires an expressed agreement from the author.
The preceding clause must be included in all derivative software developed with commerical rights retained by the author of this software.

The author takes no responsibility for the consequences of the use of this software.

Use of the software in any form indicates acknowledgement of the licensing terms.
