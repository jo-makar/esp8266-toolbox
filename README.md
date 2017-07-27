# esp8266-toolbox
Motley of servers and clients for the ESP8266 with signed OTA updates

# Status
- [x] HTTP server framework
  - Multiple simultaneous clients
  - GET and POST methods
- [x] SHA256 implementation
- [x] Push-based OTA updates
  - Triggered by HTTP POST on /fota/push
- [ ] Big integer implementation
- [ ] RSA encryption/decryption implementation

FIXME Rewrite the next two sections & work out the minimum needed

- [ ] Signed OTA updates
  - Will need PKI impl and ideally use OpenSSL on host
- [ ] Add WiFi urls: /wifi/{setup,info,scan?}
- [ ] Add /logs to show last 100 log lines in a circular buffer
- [ ] SMTP client framework for email/text alerts
- [ ] Light, motion detection sensor drivers
- [ ] Temp., humidity sensor drivers
- [ ] Battery power circuit and monitoring code
  - Use sleep modes for power conservation
- [ ] Production board design

# Later
- [ ] Pull-based OTA updates via HTTP GET
  - Scheduled checks say every six hours by timer
  - The server url will use an id for device-specific apps/keys
- [ ] Provide option of using HTTPS via SDK (espconn_secure_*)
  - Limitation of only one server socket and one client
- [ ] NTP client framework (provided by the SDK)
- [ ] MQTT client framework

# OTA updates
An unencoded binary is uploaded by HTTP POST to /fota/push.  The binary gets
written to the unused partition in flash while its hash is calculated.  When done
it is read from flash and its hash is recalculated to verify the write integrity,
if successful an HTTP 202 Accepted response is returned and the system is
rebooted into the new application.

The Makefile target fota shows how this can be done with curl.

# Provided urls
Url | Description
--- | -----------
/fota/push | Push-based OTA updates (described above)
/uptime | System uptime
/version | Application version

# Quick start
- Install the toolchain and SDK: https://github.com/pfalcon/esp-open-sdk
- Modify the variables at the top of the Makefile if necessary
- Run `make keys` to generate the OTA signing keys
- Run `make` to build the application binary
- Breadboard a circuit (refer to /hw for a development schematic)
- Power the circuit and connect via a USB-UART bridge
- Put the ESP8266 in flash mode by holding GPIO0 low during boot
- Run `make flash` to flash the bins (as root or user with appropriate perms)
- The default WiFi SSID is `esp-<system id in hex>`
- The default WiFi password is in /config.h
- The default IP address is 192.168.4.1

# Debug output
A logging system modelled after syslog is provided for output over UART0.  The
macros are defined in /log.h and system log levels set in /config.h.  An example
log snapshot follows.

    1.747: debug: wifi.c:94: softap: staconnected mac=74:f3:38:3a:fd:39 aid=01
    9.848: info: httpd/process.c:141: 192.168.4.2:49933 post len=238708 url=/fota/push
    10.931: info: httpd/url_fota.c:133: flashed sector 0x81000
    11.014: info: httpd/url_fota.c:133: flashed sector 0x82000

    other flash sector entries elided

    15.620: info: httpd/url_fota.c:133: flashed sector 0xba000
    15.691: info: httpd/url_fota.c:133: flashed sector 0xbb000
    15.691: info: httpd/url_fota.c:152: 8ed9df19463cd5f966ad0bbc17931bc661a5cc516092148a7d28f99e0cba8a14
    16.050: info: httpd/url_fota.c:192: 8ed9df19463cd5f966ad0bbc17931bc661a5cc516092148a7d28f99e0cba8a14
    21.051: info: httpd/url_fota.c:241: rebooting

The applications `uart0.py` and `uart0/uart0` are provided for debugging output.
`uart0/uart0` is for USB-UART bridges that don't inherently support non-standard
baudrates.

# Coding style
- 80 character line limits (for multiple horizontally-split editor views)
- Prefer to put function-scope variables at the top of the function; to help
  facilitate calculation of stack space used by the function

# License
This software is freely available for non-commerical use, commerical use requires
an expressed agreement from the author. The preceding clause must be included in
all derivative software developed with commerical rights retained by the author
of this software.

The author takes no responsibility for the consequences of the use of this
software.

Use of the software in any form indicates acknowledgement of the licensing terms.

Copyright (c) 2017 J. O. Makar <jomakar@gmail.com>
