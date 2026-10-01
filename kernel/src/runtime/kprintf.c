#include <stdarg.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
 
#include "minemu/kprintf.h"
#include "minemu/uart.h"
 
struct fmt_spec {
    bool left;  /* '-' flag */
    bool zero;  /* '0' flag */
    int  width; /* minimum field width */
};
 
static int emit_char(char c) {
    uart_putc(c);
    return 1;
}
 
static int emit_repeat(char c, int n) {
    int count = 0;
    while (n-- > 0) {
        count += emit_char(c);
    }
    return count;
}
 
static int emit_string(const char *s, const struct fmt_spec *sp) {
    int len = 0;
    int count = 0;
 
    if (s == NULL) {
        s = "(null)";
    }
    while (s[len]) {
        len++;
    }
 
    int pad = sp->width > len ? sp->width - len : 0;
 
    if (!sp->left) {
        count += emit_repeat(' ', pad);
    }
    for (int i = 0; i < len; i++) {
        count += emit_char(s[i]);
    }
    if (sp->left) {
        count += emit_repeat(' ', pad);
    }
    return count;
}
 
/*
 * Print an unsigned value in the given base, with optional '-' sign and
 * prefix (e.g. "0x"), honoring width / zero-pad / left-justify.
 */
static int emit_number(uint32_t v, unsigned base, bool upper, bool negative,
                       const char *prefix, const struct fmt_spec *sp) {
    static const char lower_digits[] = "0123456789abcdef";
    static const char upper_digits[] = "0123456789ABCDEF";
    const char *digits = upper ? upper_digits : lower_digits;
 
    char buf[32];
    int n = 0;
    int count = 0;
 
    do {
        buf[n++] = digits[v % base];
        v /= base;
    } while (v != 0);
 
    int prefix_len = 0;
    while (prefix && prefix[prefix_len]) {
        prefix_len++;
    }
 
    int total = n + prefix_len + (negative ? 1 : 0);
    int pad = sp->width > total ? sp->width - total : 0;
 
    /* Left-justify overrides zero-padding, as in standard printf. */
    bool zero_pad = sp->zero && !sp->left;
 
    if (!sp->left && !zero_pad) {
        count += emit_repeat(' ', pad);
    }
    if (negative) {
        count += emit_char('-');
    }
    for (int i = 0; i < prefix_len; i++) {
        count += emit_char(prefix[i]);
    }
    if (zero_pad) {
        count += emit_repeat('0', pad);
    }
    while (n > 0) {
        count += emit_char(buf[--n]);
    }
    if (sp->left) {
        count += emit_repeat(' ', pad);
    }
    return count;
}
 
int kvprintf(const char *fmt, va_list ap) {
    int count = 0;
 
    for (; *fmt; fmt++) {
        if (*fmt != '%') {
            count += emit_char(*fmt);
            continue;
        }
 
        fmt++; /* skip '%' */
        if (*fmt == '\0') {
            break; /* lone '%' at end of string */
        }
 
        struct fmt_spec sp = { false, false, 0 };
 
        /* flags */
        for (;; fmt++) {
            if (*fmt == '-') {
                sp.left = true;
            } else if (*fmt == '0') {
                sp.zero = true;
            } else {
                break;
            }
        }
 
        /* width */
        while (*fmt >= '0' && *fmt <= '9') {
            sp.width = sp.width * 10 + (*fmt - '0');
            fmt++;
        }
 
        /* length modifiers (all ints are 32-bit here) */
        while (*fmt == 'l' || *fmt == 'z' || *fmt == 'h') {
            fmt++;
        }
 
        switch (*fmt) {
        case 's':
            count += emit_string(va_arg(ap, const char *), &sp);
            break;
 
        case 'c': {
            char c = (char)va_arg(ap, int);
            char s[2] = { c, '\0' };
            /* Route through emit_string so width/padding work. */
            count += emit_string(s, &sp);
            break;
        }
 
        case '%':
            count += emit_char('%');
            break;
 
        case 'd':
        case 'i': {
            int32_t v = va_arg(ap, int32_t);
            bool neg = v < 0;
            /* Negate in unsigned space so INT32_MIN is handled correctly. */
            uint32_t mag = neg ? (uint32_t)0 - (uint32_t)v : (uint32_t)v;
            count += emit_number(mag, 10, false, neg, NULL, &sp);
            break;
        }
 
        case 'u':
            count += emit_number(va_arg(ap, uint32_t), 10, false, false, NULL, &sp);
            break;
 
        case 'x':
            count += emit_number(va_arg(ap, uint32_t), 16, false, false, NULL, &sp);
            break;
 
        case 'X':
            count += emit_number(va_arg(ap, uint32_t), 16, true, false, NULL, &sp);
            break;
 
        case 'p': {
            /* Always 0x-prefixed, zero-padded to 8 hex digits (32-bit). */
            struct fmt_spec psp = { false, true, 8 + 2 };
            uint32_t v = (uint32_t)(uintptr_t)va_arg(ap, void *);
            count += emit_number(v, 16, false, false, "0x", &psp);
            break;
        }
 
        case '\0':
            return count; /* format ended mid-specifier */
 
        default:
            /* Unknown specifier: echo it so the bug is visible. */
            count += emit_char('%');
            count += emit_char(*fmt);
            break;
        }
    }
 
    return count;
}
 
int kprintf(const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    int n = kvprintf(fmt, ap);
    va_end(ap);
    return n;
}