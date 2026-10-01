#include <stddef.h>
#include <stdint.h>
 
#include "minemu/msh.h"
#include "minemu/uart.h"
 
#define MSH_LINE_MAX 20 /* max bytes in a line, not counting '\n' */
 
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
        /* Repeated spaces after "echo" collapse into one separator. */
        uart_puts(skip_spaces(end));
        uart_putc('\n');
        return;
    }
 
    uart_puts("command not found: ");
    for (size_t i = 0; i < n; i++) {
        uart_putc(word[i]);
    }
    uart_putc('\n');
}
 
/*
 * Reads one line into buf (capacity MSH_LINE_MAX + 1) and NUL-terminates it.
 *
 * - '\n' is the only terminator.
 * - 0x08 and 0x7f erase the previous character; ignored on an empty line.
 * - Bytes past MSH_LINE_MAX are dropped (not stored, not echoed), so the
 *   buffer can never overflow. Backspace still works on what was stored.
 */
static void read_line(char *buf) {
    size_t len = 0;
 
    for (;;) {
        uint8_t c = uart_getc();
 
        if (c == '\n') {
            break;
        }
 
        if (c == 0x08 || c == 0x7f) {
            if (len > 0) {
                len--;
            }
            continue;
        }
 
        if (len < MSH_LINE_MAX) {
            buf[len++] = (char)c;
        }
    }
 
    buf[len] = '\0';
}
 
void msh_run(void) {
    char line[MSH_LINE_MAX + 1];
 
    for (;;) {
        uart_puts("msh> ");
        read_line(line);
        msh_execute(line);
    }
}