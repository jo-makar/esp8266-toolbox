# esp8266-toolbox
Motley of servers, clients and drivers for the ESP8266 with signed OTA updates

# Status
- [x] Logging framework
- [x] HTTP server framework
- [ ] OTA updates
  - [ ] RSA signed updates
      - [ ] Big integer implementation
      - [ ] SHA256 implementation
  - [ ] Push updates
  - [ ] Pull updates
- [ ] SMTP client framework
  - [ ] Provide SSL via BearSSL
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
The default for all subsystem logging levels (eg MAIN_LOG_LEVEL) is LEVEL_INFO
and can be changed at runtime with log_raise() or log_lower().

An example use case from user_init.c:
    INFO(MAIN, "Version %s built on %s", VERSION, BUILD_DATE)

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
A simple HTTP server framework that support multiple simulatenous clients is
implemented in http/.

To add new urls, add an entry in http_urls and implement the corresponding
callback.  Stubs are provided for processing POST data and writing to an output
buffer, see the many examples listed.

## Provided urls
Url | Description
--- | -----------
/logs | Most recent log entries (?refresh to refresh automatically)
/logs/lower | Lower a system's log level (eg ?main)
/logs/raise | Raise a system's log level
/ota/bin | Currently executing userbin/partition
/ota/push | Push-based OTA updates (described below)
/reset | System reset
/uptime | System uptime
/version | Application version
/wifi/setup | WiFi SSID/password setup

# License
This software is freely available for non-commerical use, commerical use requires
an expressed agreement from the author. The preceding clause must be included in
all derivative software developed with commerical rights retained by the author
of this software.

The author takes no responsibility for the consequences of the use of this
software.

Use of the software in any form indicates acknowledgement of the licensing terms.

Copyright (c) 2017 J. O. Makar <jomakar@gmail.com>
