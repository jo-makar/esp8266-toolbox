#ifndef CONFIG_H
#define CONFIG_H

#include <user_interface.h>

#define VERSION_MAJOR 1
#define VERSION_MINOR 0
#define VERSION_PATCH 0

#define BUILD_DATETIME __DATE__ " " __TIME__

#define SSID_PREFIX "esp"
#define SSID_PASS "l0cK*It_Dwn"

#define SMTP_USER "xxx@gmail.com"
#define SMTP_PASS "yyy"
/* FIXME SMTP_HOST SMTP_PASS */

/*******************************************************************************/

#define LEVEL_NOTSET    0
#define LEVEL_DEBUG    10
#define LEVEL_INFO     20
#define LEVEL_WARNING  30
#define LEVEL_ERROR    40
#define LEVEL_CRITICAL 50

#define CRYPTO_LOG_LEVEL LEVEL_INFO
#define FOTA_LOG_LEVEL   LEVEL_INFO
#define HTTPD_LOG_LEVEL  LEVEL_INFO
#define MAIN_LOG_LEVEL   LEVEL_DEBUG
#define SMTP_LOG_LEVEL   LEVEL_DEBUG

#define LOG_URLBUF_ENABLE 1

#endif
