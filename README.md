# esp8266-toolbox
Motley of servers, clients and drivers for the ESP8266 with signed OTA updates

# Status
- [x] Logging framework (akin to syslog)
- [ ] HTTP server framework
  - [ ] SSL via BearSSL or similar 
- [ ] OTA updates
  - [ ] RSA signed updates
      - [ ] Big integer implementation
      - [ ] SHA256 implementation
  - [ ] Push updates
  - [ ] Pull updates
- [ ] SMTP client framework
- [ ] Environment monitoring
  - [ ] Ambient light
  - [ ] Motion, vibration
  - [ ] Temperature, humidity
- [ ] Battery circuit monitoring
- [ ] Sleep modes for power conservation
- [ ] Self-diagnosis and healing
- [ ] MQTT client framework
- [ ] Production board design

# Quick start
- Install the toolchain and SDK: https://github.com/pfalcon/esp-open-sdk
- Modify the Makefile variables in config.mk if necessary
- Run `make` to build the application binaries
- Power the circuit and connect via a USB-UART bridge
- Put the ESP8266 in flash mode by holding GPIO0 low during boot
- Run `make flash` to flash the application over USB/UART0
- The default WiFi SSID is `esp8266-<sysid in hex>`
- The default WiFi password is in config.h
- The default WiFi access point IP address is 192.168.4.1
- Go to http://192.168.4.1/wifi/setup to set the WiFi station config

# Logging framework
A logging framework akin to Linux's syslog(3) is available from log/log.h.
Subsystem logging levels (eg MAIN_LOG_LEVEL) are intended to be defined in
config.h using the LEVEL_* definitions.

An example use case from user_init.c:
    info(MAIN, "Version %s built on %s", VERSION, BUILD_DATE)

Would produce the following log entry:
    00:00:15.506: info: user_init.c:13: Version 1.0.0 built on Aug 10 2017 06:52:39
which is comprised of system time (hours:minutes:seconds.milliseconds), log
level, file path and line number and finally entry proper.

The log entries are sent to UART0 (with the default baudrate and params) and
stored in a buffer for use elsewhere, eg served by web page or sent by mail.

Run `make log` to execute a host application that will read the output produced
from UART0 including the generated log entries.  The Linux application
log/uart0/uart0 is provided to support USB-UART bridges that can use non-standard
baudrates only via ioctl() calls, eg the CP2104.

# HTTP server framework
FIXME List provided urls
FIXME Provide text and html versions of most if not all

# License
This software is freely available for non-commerical use, commerical use requires
an expressed agreement from the author. The preceding clause must be included in
all derivative software developed with commerical rights retained by the author
of this software.

The author takes no responsibility for the consequences of the use of this
software.

Use of the software in any form indicates acknowledgement of the licensing terms.

Copyright (c) 2017 J. O. Makar <jomakar@gmail.com>
