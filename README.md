# esp8266-toolbox
Motley of servers, clients and drivers for the ESP8266 with signed OTA updates

# Status
- [x] HTTP server framework
  - [ ] Refactor to use a state-machine design
    - Intended to simplify the framework interface (ie hide the SDK callbacks)
    - Add support for multiple sends per HTTP response (eg http_url_logs())
    - Data is to be pushed/pulled with an underlying task handling the callbacks
      - The data exchanges are to be asynchronous and non-blocking
- [x] Over-The-Air updates
  - [x] SHA256 implementation
  - [x] RSA signed updates
    - [x] Big integer implementation
  - [x] Push updates
  - [ ] Pull updates
- [x] SMTP client framework
  - [x] Flash param storage/retrieval
  - [x] CRAM-MD5 authentication
  - [ ] Refactor to use a state-machine design
  - [ ] SSL support (via BearSSL or similar)
- [x] Logging framework
  - [ ] Use SNTP for timestamps if available
- [ ] Environment monitoring
  - [x] Ambient light
    - [x] TLS2561 light sensing
      - [ ] Add support for interrupt based on significant change
  - [ ] Motion, vibration
  - [ ] Temperature, humidity
- [ ] Battery circuit monitoring
- [ ] Sleep modes for power conservation
- [ ] Self-diagnosis and healing
  - [ ] Monitor heap usage and stack pointer
- [ ] MQTT client framework
- [x] Custom board design

# Quick start
- Install the toolchain and SDK: https://github.com/pfalcon/esp-open-sdk
- Modify the Makefile variables in config.mk if necessary
- Run `make keys` to generate the OTA signing keys
- Run `make` to build the application binaries
- Power the circuit and connect via a USB-UART bridge
- Put the ESP8266 in flash mode by holding GPIO0 low during boot
- Run `make flash` to flash the application over USB/UART0
- The default WiFi SSID is `esp8266-<sysid in hex>`
- The default WiFi password is in config.h
- The default WiFi access point IP address is 192.168.4.1
- Go to http://192.168.4.1/wifi/setup to set the WiFi station config
- Go to http://192.168.4.1/smtp/setup to set the SMTP account config

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
/smtp/setup | SMTP account setup
/uptime | System uptime
/version | Application version
/wifi/setup | WiFi SSID/password setup

# OTA updates
An unencoded binary is uploaded by HTTP POST to /fota/push. The binary gets
written to the unused partition in flash while its hash (SHA256) is calculated.
When done it is read from flash and its hash is recalculated to verify the write
integrity.

The binary's hash is encrypted with a private RSA key and included in the URL to
serve as a signature. If the signature does not verify with the embedded public
key the firmware is rejected.

If successful a HTTP 202 Accepted response is returned and the system is rebooted
into the new application otherwise a 400 Bad Request is returned.

Run `make ota` to flash the binary using the procedure described.

The big integer implementation only supports unsigned operations but it sufficient
for RSA encryption and decryption.

# SMTP client framework
A simple SMTP client for sending mails and texts is available in smtp/.

Currently only supporting non-SSL/TLS SMTP providers (eg SMTP2GO).

[BearSSL](http://www.bearssl.org) support forthcoming, to rebuild run
`(cd bearssl-0.5; make CONF=esp8266)`.

The account information (host, port, etc) are stored in flash with other user
params starting at three sectors before the second partition.  Reference the code
in param/ for background on user param storage and retrieval.

## Mail triggers
- After connecting to the configured AP (ie system online)
- The uptime day count is incremented (ie once every 24 hours)

# Logging framework
A logging framework akin to Linux's syslog(3) is available from log/log.h.
The default for all subsystem logging levels (eg MAIN_LOG_LEVEL) is LEVEL_INFO
and can be changed at runtime with log_raise() or log_lower().

An example use case from user_init.c:

`INFO(MAIN, "Version %s built on %s", VERSION, BUILD_DATE)`

Would produce the following log entry:

`00:00:15.506: info: user_init.c:13: Version 1.0.0 built on Aug 10 2017 06:52:39`

Which is comprised of system time (hours:minutes:seconds.milliseconds), log
level, file path and line number and finally entry proper.

The log entries are sent to UART0 (with the default baudrate and params) and
stored in a buffer for use elsewhere, eg served by web page or sent by mail.

Run `make log` to execute a host application that will read the output produced
from UART0 including the generated log entries.  The Linux application
log/uart0/uart0 is provided to support USB-UART bridges that can use
non-standard baudrates only via ioctl() calls, eg the CP2104.

# Environment monitoring

The targeted ESP8266-01 has all of its pins used during the startup process but
four (RX, TX, GPIO0 and GPIO2) can be later multiplexed, e.g. say for
(bitbanged) I2C or SPI.

Sensors can be attached to implement environment monitoring using those buses.
Supported device drivers are provided in drivers/.

# Custom board design

A custom board was designed to simplify development, the schematic and gerbers
are in board/.  The schematic is shown below.

[[https://github.com/jo-makar/esp8266-toolbox/blob/master/board/board.pdf]]

[[board/board.pdf]]

# License
This software is freely available for non-commerical use, commerical use requires
an expressed agreement from the author. The preceding clause must be included in
all derivative software developed with commerical rights retained by the author
of this software.

The author takes no responsibility for the consequences of the use of this
software.

Use of the software in any form indicates acknowledgement of the licensing terms.

Copyright (c) 2017 J. O. Makar <jomakar@gmail.com>
