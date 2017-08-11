#ifndef UPTIME_H
#define UPTIME_H

uint64_t uptime_us();

extern os_timer_t uptime_timer;
void uptime_handler(void *arg);

#endif
