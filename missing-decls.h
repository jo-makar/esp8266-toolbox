#ifndef MISSING_DECLS_H
#define MISSING_DECLS_H

void ets_install_putc1(void *);

void ets_isr_attach(int, void *, void *);
void ets_isr_mask(unsigned int);
void ets_isr_unmask(unsigned int);

int ets_uart_printf(const char *, ...);

int os_printf_plus(const char *, ...);

void uart_div_modify(int, unsigned int);

#endif
