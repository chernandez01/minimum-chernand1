#ifndef MINEMU_IRQ_H
#define MINEMU_IRQ_H

#include "minemu/trap.h"
#include <stdint.h>

/* Students implement this A32 vector trampoline and its IRQ dispatch policy. */
void minemu_irq_trampoline(void) __attribute__((noreturn));
struct minemu_trap_frame *minemu_irq_dispatch(struct minemu_trap_frame *frame);

static inline void minemu_irq_enable(void) {
    __asm__ volatile("cpsie i" : : : "memory");
}

static inline void minemu_irq_disable(void) {
    __asm__ volatile("cpsid i" : : : "memory");
}

typedef void (*minemu_irq_handler_t)(void);
 
#define MINEMU_IRQ_MAX_SOURCES 32u
 
/* Returns 0 on success, -1 if id is out of range. */
int minemu_irq_register(uint32_t id, minemu_irq_handler_t handler);
 
/* Unmask a peripheral source in the interrupt controller. */
void minemu_irq_enable_source(uint32_t id);

#endif
