#include <stddef.h>
#include <stdint.h>
 
#include <minemu/trap.h>
#include "minemu/irq.h"

#include <minemu/trace.h>
 
#define INTC_ENABLE (*(volatile uint32_t *)0x10000004u)
#define INTC_EOI    (*(volatile uint32_t *)0x1000000Cu)
 
static minemu_irq_handler_t handlers[MINEMU_IRQ_MAX_SOURCES];
 
int minemu_irq_register(uint32_t id, minemu_irq_handler_t handler) {
    if (id >= MINEMU_IRQ_MAX_SOURCES) {
        return -1;
    }
    handlers[id] = handler;
    return 0;
}
 
void minemu_irq_enable_source(uint32_t id) {
    if (id < MINEMU_IRQ_MAX_SOURCES) {
        INTC_ENABLE |= (UINT32_C(1) << id);
    }
}


struct minemu_trap_frame *minemu_irq_dispatch(struct minemu_trap_frame *frame) {
    int32_t id = frame->exception_id;

 
    if (id >= 0 && id < (int32_t)MINEMU_IRQ_MAX_SOURCES) {
        minemu_irq_handler_t handler = handlers[id];
        if (handler != NULL) {
            handler();
        }
        /* EOI only after the handler has cleared the device's own
         * interrupt condition, otherwise it re-fires immediately. */
        INTC_EOI = (uint32_t)id;
    }
 
    /* Same frame for now; a scheduler may return a different one later. */
    return frame;
}
 