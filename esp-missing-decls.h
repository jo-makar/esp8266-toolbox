#ifndef ESP_MISSING_DECLS_H
#define ESP_MISSING_DECLS_H

#include <stddef.h>

/* Gleaned from various sources (eg https://github.com/mziwisky/esp8266-dev/blob/master/esphttpd/include/espmissingincludes.h) */

#define os_snprintf ets_snprintf

int os_printf_plus(const char *format, ...)  __attribute__ ((format (printf, 1, 2)));
int os_sprintf(char *str, const char *format, ...) __attribute__ ((format (printf, 2, 3)));

void uart_div_modify(int no, unsigned int freq);

void   ets_bzero(void *s, size_t n);
int    ets_memcmp(const void *s1, const void *s2, size_t n);
void  *ets_memcpy(void *dest, const void *src, size_t n);
void  *ets_memmove(void *dest, const void *src, size_t n);
int    ets_snprintf(char *str, size_t size, const char *format, ...)  __attribute__ ((format (printf, 3, 4)));
int    ets_sprintf(char *str, const char *format, ...)  __attribute__ ((format (printf, 2, 3)));
int    ets_strcmp(const char *s1, const char *s2);
char  *ets_strcpy(char *dest, const char *src);
size_t ets_strlen(const char *s);
int    ets_strncmp(const char *s1, const char *s2, int len);
char  *ets_strncpy(char *dest, const char *src, size_t n);

#endif
