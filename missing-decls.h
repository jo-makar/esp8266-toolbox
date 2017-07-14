#ifndef MISSING_DECLS_H
#define MISSING_DECLS_H

void ets_delay_us(uint16);

void ets_install_putc1(void *);

void ets_isr_attach(int, void *, void *);
void ets_isr_mask(unsigned int);
void ets_isr_unmask(unsigned int);

int ets_sprintf(char *, const char *, ...);
int ets_snprintf(char *, size_t, const char *, ...);

size_t ets_strlen(const char *);

int ets_strncmp(const char *, const char *, size_t);
char *ets_strncpy(char *, const char *, size_t n);

int ets_uart_printf(const char *, ...);

int os_printf_plus(const char *, ...);

#define os_snprintf ets_snprintf

void uart_div_modify(unsigned int, unsigned int);

#endif
