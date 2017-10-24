#ifndef CONFIG_H
#define CONFIG_H

#define VERSION_MAJOR 1
#define VERSION_MINOR 0
#define VERSION_PATCH 0

#define _STR(s) __STR(s)
#define __STR(s) #s
#define VERSION _STR(VERSION_MAJOR) "." _STR(VERSION_MINOR) "." _STR(VERSION_PATCH)

#define BUILD_DATETIME __DATE__ " " __TIME__

#define WIFI_SSID "xxx"
#define WIFI_PASS "yyy"

#define SMTP_HOST "xxx"
#define SMTP_PORT 25
#define SMTP_USER "yyyy"
#define SMTP_PASS "zzzz"
#define SMTP_FROM "aaaa"

#endif
