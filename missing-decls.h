#ifndef MISSING_DECLS_H
#define MISSING_DECLS_H

#include <user_interface.h>

void ets_bzero(void *, size_t);

void ets_delay_us(uint16);

void *ets_memcpy(void *, const void *, size_t);
void *ets_memmove(void *, const void *, size_t);

int ets_sprintf(char *, const char *, ...);
int ets_snprintf(char *, size_t, const char *, ...);

size_t ets_strlen(const char *);

int ets_strcmp(const char *, const char *);
int ets_strncmp(const char *, const char *, size_t);

char *ets_strncpy(char *, const char *, size_t n);

char *ets_strstr(const char *, const char *);

void ets_timer_arm_new(os_timer_t *, uint32_t, bool, int);
void ets_timer_disarm(os_timer_t *);
void ets_timer_setfn(os_timer_t *, os_timer_func_t *, void *);

int os_printf_plus(const char *, ...);

#define os_snprintf ets_snprintf

void *pvPortMalloc(size_t, const char *, unsigned int);

void vPortFree(void *, const char *, unsigned int);

#endif
