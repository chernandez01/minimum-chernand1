#ifndef MINEMU_MSH_H
#define MINEMU_MSH_H
 
/* Runs the kernel shell forever. Requires uart_init() and CPU IRQs
 * to have been enabled first. Never returns. */
void msh_run(void) __attribute__((noreturn));
 
#endif
 