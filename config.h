#ifndef CONFIG_H
#define CONFIG_H

#define VERSION_MAJOR 1
#define VERSION_MINOR 0
#define VERSION_PATCH 0

#define _STR(s) __STR(s)
#define __STR(s) #s
#define VERSION _STR(VERSION_MAJOR) "." _STR(VERSION_MINOR) "." _STR(VERSION_PATCH)

#define BUILD_DATETIME __DATE__ " " __TIME__

#endif
