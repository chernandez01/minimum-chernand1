#include <stdbool.h>
#include <stdint.h>

#include "minemu/platform.h"
#include "minemu/syscall.h"
#include "minemu/trap.h"
#include "minemu/uart.h"
#include "minemu/user.h"
#include "minemu/user_abi.h"

#define CPSR_MODE_MASK UINT32_C(0x1f)
#define CPSR_MODE_USR UINT32_C(0x10)
#define CPSR_THUMB UINT32_C(0x20)
#define A32_SVC_ZERO UINT32_C(0xef000000)

/* ioctl(fd, UART_WRITE, src, n): write n bytes from user memory to a UART.
 * Every check happens before the first byte is sent. */
static int32_t ioctl_uart_write(uint32_t fd, uint32_t src, uint32_t n) {
    if (fd != MINEMU_FD_UART_OUT) { /* UART0 only for now */
        return MINEMU_IOCTL_ERROR;
    }
    if (n > MINEMU_IOCTL_MAX_TRANSFER || !minemu_user_range_ok(src, n, false)) {
        return MINEMU_IOCTL_ERROR;
    }

    const uint8_t *bytes = (const uint8_t *)(uintptr_t)src;
    for (uint32_t i = 0; i < n; ++i) {
        uart_putc((char)bytes[i]);
    }
    return (int32_t)n;
}

/* r0 = fd, r1 = request, r2/r3 = request-specific arguments. */
static int32_t do_ioctl(const struct minemu_trap_frame *frame) {
    uint32_t fd = frame->r[0];
    uint32_t request = frame->r[1];
    uint32_t arg0 = frame->r[2];
    uint32_t arg1 = frame->r[3];

    switch (request) {
    case MINEMU_IOCTL_UART_WRITE:
        return ioctl_uart_write(fd, arg0, arg1);
    default:
        return MINEMU_IOCTL_ERROR;
    }
}

struct minemu_trap_frame *minemu_svc_dispatch(struct minemu_trap_frame *frame) {
    /* Must be a genuine A32 `svc #0` from user mode. */
    if (frame->exception_id != MINEMU_EXCEPTION_SVC ||
        (frame->spsr & CPSR_MODE_MASK) != CPSR_MODE_USR ||
        (frame->spsr & CPSR_THUMB) != 0 ||
        *(const uint32_t *)(uintptr_t)frame->fault_pc != A32_SVC_ZERO) {
        frame->r[0] = (uint32_t)MINEMU_IOCTL_ERROR;
        return frame;
    }

    int32_t result;
    switch (frame->r[7]) {
    case MINEMU_SYSCALL_IOCTL:
        result = do_ioctl(frame);
        break;
    default:
        result = MINEMU_IOCTL_ERROR;
        break;
    }

    frame->r[0] = (uint32_t)result; /* restored into r0 on return to user */
    return frame;
}