#include <stdbool.h>
#include <stdint.h>

#include "minemu/platform.h"
#include "minemu/syscall.h"
#include "minemu/trace.h"
#include "minemu/trap.h"
#include "minemu/uart.h"
#include "minemu/user.h"
#include "minemu/user_abi.h"

#define CPSR_MODE_MASK UINT32_C(0x1f)
#define CPSR_MODE_USR UINT32_C(0x10)
#define CPSR_THUMB UINT32_C(0x20)
#define A32_SVC_ZERO UINT32_C(0xef000000)

/* UART fds: 0 and 1 both name UART0 (kernel RX ring buffer + TX register);
 * 2 names UART1, which has no driver, so its registers are used directly. */
static bool fd_is_uart(uint32_t fd) {
    return fd == MINEMU_FD_UART_IN || fd == MINEMU_FD_UART_OUT ||
           fd == MINEMU_FD_UART_DIAG;
}

/* ioctl(fd, UART_READ, dst, n): copy up to n bytes that are already waiting
 * into user memory. Never blocks; returns the byte count (0 if none). */
static int32_t ioctl_uart_read(uint32_t fd, uint32_t dst, uint32_t n) {
    if (!fd_is_uart(fd) || n > MINEMU_IOCTL_MAX_TRANSFER ||
        !minemu_user_range_ok(dst, n, true)) {
        return MINEMU_IOCTL_ERROR;
    }

    uint8_t *out = (uint8_t *)(uintptr_t)dst;
    uint32_t count = 0;
    if (fd == MINEMU_FD_UART_DIAG) {
        while (count < n && (MINEMU_UART1->status & MINEMU_UART_STATUS_RX_READY) != 0) {
            out[count++] = (uint8_t)MINEMU_UART1->rx_data;
        }
    } else {
        uart_poll(); /* do not depend on the RX IRQ firing in user mode */
        while (count < n && uart_try_getc(&out[count])) {
            ++count;
        }
    }
    return (int32_t)count;
}

/* ioctl(fd, UART_WRITE, src, n): write n bytes from user memory to a UART.
 * Every check happens before the first byte is sent. */
static int32_t ioctl_uart_write(uint32_t fd, uint32_t src, uint32_t n) {
    if (!fd_is_uart(fd) || n > MINEMU_IOCTL_MAX_TRANSFER ||
        !minemu_user_range_ok(src, n, false)) {
        return MINEMU_IOCTL_ERROR;
    }

    const uint8_t *bytes = (const uint8_t *)(uintptr_t)src;
    for (uint32_t i = 0; i < n; ++i) {
        if (fd == MINEMU_FD_UART_DIAG) {
            while ((MINEMU_UART1->status & MINEMU_UART_STATUS_TX_READY) == 0) {
            }
            MINEMU_UART1->tx_data = bytes[i];
        } else {
            uart_putc((char)bytes[i]);
        }
    }
    return (int32_t)n;
}

/* RNG requests that store a 32-bit value through a user pointer. The pointer
 * is validated before the device is touched, so a bad pointer cannot advance
 * the generator. */
static int32_t ioctl_rng_load(uint32_t request, uint32_t dst) {
    if ((dst & 3U) != 0 || !minemu_user_range_ok(dst, sizeof(uint32_t), true)) {
        return MINEMU_IOCTL_ERROR;
    }
    uint32_t value = (request == MINEMU_IOCTL_RNG_NEXT) ? MINEMU_RNG->data
                                                        : MINEMU_RNG->state;
    *(uint32_t *)(uintptr_t)dst = value;
    return 0;
}

/* r0 = fd, r1 = request, r2/r3 = request-specific arguments. */
static int32_t do_ioctl(const struct minemu_trap_frame *frame) {
    uint32_t fd = frame->r[0];
    uint32_t request = frame->r[1];
    uint32_t arg0 = frame->r[2];
    uint32_t arg1 = frame->r[3];

    switch (request) {
    case MINEMU_IOCTL_UART_READ:
        return ioctl_uart_read(fd, arg0, arg1);
    case MINEMU_IOCTL_UART_WRITE:
        return ioctl_uart_write(fd, arg0, arg1);
    case MINEMU_IOCTL_RNG_NEXT:
    case MINEMU_IOCTL_RNG_STATE:
        return fd == MINEMU_FD_RNG ? ioctl_rng_load(request, arg0) : MINEMU_IOCTL_ERROR;
    case MINEMU_IOCTL_RNG_SEED:
        if (fd != MINEMU_FD_RNG) {
            return MINEMU_IOCTL_ERROR;
        }
        MINEMU_RNG->seed = arg0;
        return 0;
    case MINEMU_IOCTL_TRACE_EVENT:
        if (fd != MINEMU_FD_TRACE) {
            return MINEMU_IOCTL_ERROR;
        }
        minemu_trace_event(arg0);
        return 0;
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