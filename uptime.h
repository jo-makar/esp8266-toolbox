#ifndef UPTIME_H
#define UPTIME_H

uint64_t uptime_us();

extern os_timer_t uptime_overflow_timer;
void uptime_overflow_handler(void *arg);

#endif
