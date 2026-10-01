#ifndef MINEMU_UART_H
#define MINEMU_UART_H

#include <stdbool.h>
#include <stdint.h>

void uart_init(void);
void uart_putc(char c);
void uart_puts(const char *s);

bool uart_try_getc(uint8_t *out);
uint8_t uart_getc(void);

#endif