# esp8266-toolbox
Motley of servers, clients and drivers for the ESP8266 with signed OTA updates

# Status
- [ ] Logging framework (akin to syslog)
- [ ] HTTP server framework
  - [ ] SSL via BearSSL or similar 
- [ ] SMTP client framework
- [ ] OTA updates
  - [ ] RSA signed updates
      - [ ] Big integer implementation
      - [ ] SHA256 implementation
  - [ ] Push updates
  - [ ] Pull updates
- [ ] Battery circuit monitoring
- [ ] Sleep modes for power conservation
- [ ] Environment monitoring
  - [ ] Ambient light
  - [ ] Motion, vibration
  - [ ] Temperature, humidity
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
FIXME

# HTTP server framework
FIXME List provided urls

# License
This software is freely available for non-commerical use, commerical use requires
an expressed agreement from the author. The preceding clause must be included in
all derivative software developed with commerical rights retained by the author
of this software.

The author takes no responsibility for the consequences of the use of this
software.

Use of the software in any form indicates acknowledgement of the licensing terms.

Copyright (c) 2017 J. O. Makar <jomakar@gmail.com>
