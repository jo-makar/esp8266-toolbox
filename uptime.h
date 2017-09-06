#ifndef UPTIME_H
#define UPTIME_H

#include <stdint.h>

/*
 * The SDK function system_get_time() overflows about every 71 mins.
 * The 64-bit return value here has an overflow of >500k years.
 */
uint64_t uptime_us();

#endif
