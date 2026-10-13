#include <stddef.h>
#include <stdint.h>

#include "minemu/user_abi.h"

/* msh: the user-mode shell. All I/O goes through ioctl() on the UART fds. */

/* The spec requires lines of up to 20 bytes to work; the public tests send
 * longer ones (echo: 25 bytes) and still expect them to be handled, and a
 * 130-byte line to be dropped. So the buffer is larger than 20. */
#define MSH_LINE_MAX 64 /* max bytes in a line, not counting '\n' */

/* ---- console I/O ------------------------------------------------------ */

static void out_write(const char *s, size_t n) {
    if (n != 0) {
        (void)ioctl(MINEMU_FD_UART_OUT, MINEMU_IOCTL_UART_WRITE, (void *)s,
                    (uint32_t)n);
    }
}

static void out_puts(const char *s) {
    size_t n = 0;
    while (s[n] != '\0') {
        n++;
    }
    out_write(s, n);
}

/* UART_READ never blocks, so poll until a byte shows up. The kernel's UART
 * RX interrupt keeps filling its ring buffer while we spin in user mode. */
static uint8_t in_getc(void) {
    uint8_t c;
    for (;;) {
        if (ioctl(MINEMU_FD_UART_IN, MINEMU_IOCTL_UART_READ, (void *)&c, 1U) == 1) {
            return c;
        }
    }
}

/* ---- line editing ----------------------------------------------------- */

/* Reads one line into buf (capacity MSH_LINE_MAX + 1) and NUL-terminates it.
 * - '\n' is the only terminator.
 * - 0x08 and 0x7f erase the previous stored character; ignored on an empty line.
 * - If the line exceeds MSH_LINE_MAX, the extra bytes are dropped and the
 *   whole line is discarded at '\n' (buf becomes empty: no command runs and
 *   nothing is printed besides the next prompt). The buffer never overflows. */
static void read_line(char *buf) {
    size_t len = 0;
    int overlong = 0;

    for (;;) {
        uint8_t c = in_getc();

        if (c == '\n') {
            break;
        }
        if (c == 0x08 || c == 0x7f) {
            if (len > 0 && !overlong) {
                len--;
            }
            continue;
        }
        if (len < MSH_LINE_MAX) {
            buf[len++] = (char)c;
        } else {
            overlong = 1;
        }
    }
    buf[overlong ? 0 : len] = '\0';
}

/* ---- command execution ------------------------------------------------ */

static const char *skip_spaces(const char *s) {
    while (*s == ' ') {
        s++;
    }
    return s;
}

/* True if the n-byte word at p equals the NUL-terminated string lit. */
static int word_equals(const char *p, size_t n, const char *lit) {
    size_t i = 0;
    while (i < n && lit[i] != '\0') {
        if (p[i] != lit[i]) {
            return 0;
        }
        i++;
    }
    return i == n && lit[i] == '\0';
}

static void msh_execute(const char *line) {
    const char *word = skip_spaces(line);

    if (*word == '\0') {
        return; /* empty or all-space line */
    }

    const char *end = word;
    while (*end != '\0' && *end != ' ') {
        end++;
    }
    size_t n = (size_t)(end - word);

    if (word_equals(word, n, "echo")) {
        /* Arguments are re-joined with single spaces: runs of spaces collapse
         * and trailing spaces are dropped (matches tests/hw2/msh-editing). */
        const char *p = skip_spaces(end);
        char out[MSH_LINE_MAX + 2];
        size_t len = 0;
        while (*p != '\0') {
            if (*p == ' ') {
                p = skip_spaces(p);
                if (*p != '\0') {
                    out[len++] = ' ';
                }
            } else {
                out[len++] = *p++;
            }
        }
        out[len++] = '\n';
        out_write(out, len); /* one syscall: text + newline */
        return;
    }

    char out[MSH_LINE_MAX + 21];
    static const char prefix[] = "command not found: ";
    size_t len = 0;
    for (size_t i = 0; i < sizeof(prefix) - 1U; i++) {
        out[len++] = prefix[i];
    }
    for (size_t i = 0; i < n; i++) {
        out[len++] = word[i];
    }
    out[len++] = '\n';
    out_write(out, len);
}

void minemu_user_main(void) {
    char line[MSH_LINE_MAX + 1];

    for (;;) {
        out_puts("msh> ");
        read_line(line);
        msh_execute(line);
    }
}