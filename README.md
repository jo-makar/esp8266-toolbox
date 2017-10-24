# esp8266-toolbox
Motley of servers, clients and drivers for the ESP8266

# Status
- [ ] HTTP server framework
  - How to practically provide a server while in station-only mode?
  - State-machine design (ie push/pull data with callbacks handled by a task)
- [ ] Over-The-Air updates
  - [ ] RSA signed firmware 
  - [ ] Push updates using HTTP/S server
  - [ ] Pull updates using HTTP/S client
- [ ] SMTP client framework
  - [ ] PLAIN, CRAM-MD5 authentication
  - [ ] SSL support (via BearSSL or similar)
- [x] Logging framework inspired by syslog
  - [ ] Store entries in a circular buffer (to be sent by smtp/http)
  - [ ] Use SNTP for timestamps if available
- [ ] Environment monitoring
  - [ ] Ambient light
    - [ ] TLS2561 light sensing
    - [ ] Add interrupt support
  - [ ] Motion, vibration
  - [ ] Temperature, humidity
- [x] Custom board design
- [ ] Battery circuit monitoring
- [ ] Sleep mode for power conservation
- [ ] Self-diagnosis and healing
  - [ ] Monitor heap usage and stack pointer
- [ ] MQTT client framework

# Quick start
- Install the toolchain and SDK: https://github.com/pfalcon/esp-open-sdk
- Modify the Makefile variables in config.mk if necessary
- Modify the WiFi, SMTP, etc constants in config.h for your installation
- Run `make` to build the application binaries
- Power the circuit and connect via a USB-UART bridge
- Put the ESP8266 in flash mode by holding GPIO0 low during boot
- Run `make flash` to flash the application over USB/UART0

# Hardware
This software was intended for use with the common eight-pin ESP-12, though it
will work with any ESP8266 module with only minor modifications.

# Logging framework
A logging framework akin to Linux's syslog(3) is available via log.h.

An example use case from user_init.c:

`log_info("main", "Version %s built on %s", VERSION, BUILD_DATETIME);`

Will produce a log entry similar to the following:

`00:00:15.506: info: user_init.c:13: Version 1.0.0 built on Aug 10 2017 06:52:39`

Which is comprised of the system time (hours:minutes:seconds.milliseconds), log
level, file path and line number and finally entry proper.

The log entries are sent to UART0 (with the default baudrate and params).

Run `make uart0` to execute a host application that will read the output
produced from UART0 including the generated log entries. The host application
is provided to support USB-UART bridges that can use non-standard baudrates only
via ioctl() calls, eg the CP2104.

# Custom board design
A simple, two-layer breakout board was designed to simplify development, the
schematic and gerbers are located in board/.

# License
This software is freely available for non-commerical use, commerical use requires
an expressed agreement from the author. The preceding clause must be included in
all derivative software developed with commerical rights retained by the author
of this software.

The author takes no responsibility for the consequences of the use of this
software.

Use of the software in any form indicates acknowledgement of the licensing terms.

Copyright (c) 2017 J. O. Makar <jomakar@gmail.com>
