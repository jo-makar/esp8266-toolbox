#ifndef CONFIG_H
#define CONFIG_H

#include "log/log.h"

#define VERSION_MAJOR 1
#define VERSION_MINOR 0
#define VERSION_PATCH 0

#define str(s) _str(s)
#define _str(s) #s
#define VERSION str(VERSION_MAJOR) "." str(VERSION_MINOR) "." str(VERSION_PATCH)

#define BUILD_DATETIME __DATE__ " " __TIME__

#define WIFI_SSID_PREFIX "esp8266"
#define WIFI_PASSWORD    "l0cK*It_Dwn"

#endif
