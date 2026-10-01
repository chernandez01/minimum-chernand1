#include <minemu/platform.h>
#include <minemu/uart.h>
#include <minemu/irq.h>
#include <minemu/uart.h>
#include <minemu/trace.h>

#include <stdbool.h>
#include <stdint.h>

#ifndef MINEMU_UART_STATUS_RX_READY
#define MINEMU_UART_STATUS_RX_READY (UINT32_C(1) << 0)
#endif
#ifndef MINEMU_UART_CONTROL_RX_IRQ_ENABLE
#define MINEMU_UART_CONTROL_RX_IRQ_ENABLE (UINT32_C(1) << 0)
#endif
 
#define RX_BUF_SIZE 256u /* power of two */
#define RX_BUF_MASK (RX_BUF_SIZE - 1u)
 
static uint8_t rx_buf[RX_BUF_SIZE];
static volatile uint32_t rx_head; /* written by the IRQ handler */
static volatile uint32_t rx_tail; /* written by uart_try_getc */

// saving 

static inline uint32_t irq_save(void) {
    uint32_t cpsr;
    __asm__ volatile("mrs %0, cpsr\n\tcpsid i" : "=r"(cpsr) : : "memory");
    return cpsr;
}
 
static inline void irq_restore(uint32_t cpsr) {
    if (!(cpsr & 0x80u)) { /* I bit clear => IRQs were enabled */
        __asm__ volatile("cpsie i" : : : "memory");
    }
}

// sending

void uart_putc(char c) {
    while (!(MINEMU_UART0->status & MINEMU_UART_STATUS_TX_READY)) {
    }

    MINEMU_UART0->tx_data = (uint32_t)c;
}

void uart_puts(const char *s) {
    while (*s) {
        uart_putc(*s++);
    }
}

// receiving

static void uart0_rx_isr(void) {
    while (MINEMU_UART0->status & MINEMU_UART_STATUS_RX_READY) {
        uint8_t c = (uint8_t)MINEMU_UART0->rx_data;
        uint32_t next = (rx_head + 1u) & RX_BUF_MASK;
        if (next != rx_tail) {
            rx_buf[rx_head] = c;
            rx_head = next;
        }
        /* else: buffer full, drop the byte (but still drain the FIFO) */
    }
}

bool uart_try_getc(uint8_t *out) {
    bool got = false;
    uint32_t flags = irq_save();
 
    if (rx_tail != rx_head) {
        *out = rx_buf[rx_tail];
        rx_tail = (rx_tail + 1u) & RX_BUF_MASK;
        got = true;
    }
 
    irq_restore(flags);
    return got;
}
 
uint8_t uart_getc(void) {
    uint8_t c;
    while (!uart_try_getc(&c)) {
        /* spin; interrupts are enabled between attempts */
    }
    return c;
}
 
void uart_init(void) {
    rx_head = 0;
    rx_tail = 0;
 
    minemu_irq_register(MINEMU_IRQ_UART0, uart0_rx_isr);
 
    /* Enable at the device, then at the interrupt controller. */
    MINEMU_UART0->control |= MINEMU_UART_CONTROL_RX_IRQ_ENABLE;
    minemu_irq_enable_source(MINEMU_IRQ_UART0);
}