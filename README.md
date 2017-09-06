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
- [ ] Logging framework inspired by syslog
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
- Run `make` to build the application binaries
- Power the circuit and connect via a USB-UART bridge
- Put the ESP8266 in flash mode by holding GPIO0 low during boot
- Run `make flash` to flash the application over USB/UART0

# License
This software is freely available for non-commerical use, commerical use requires
an expressed agreement from the author. The preceding clause must be included in
all derivative software developed with commerical rights retained by the author
of this software.

The author takes no responsibility for the consequences of the use of this
software.

Use of the software in any form indicates acknowledgement of the licensing terms.

Copyright (c) 2017 J. O. Makar <jomakar@gmail.com>
