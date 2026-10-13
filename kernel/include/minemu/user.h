#ifndef MINEMU_USER_H
#define MINEMU_USER_H
 
#include <stdbool.h>
#include <stdint.h>
 
#include "minemu/boot.h"
 
#define MINEMU_USER_IMAGE_BASE UINT32_C(0x00400000)
#define MINEMU_USER_STACK_BOTTOM UINT32_C(0x007ff000)
#define MINEMU_USER_STACK_TOP UINT32_C(0x00800000)
 
/* Maps the system ROM, finds the named module, and loads it into a fresh user
 * address space (code, data, bss, and a stack). On success stores the module's
 * entry address in *entry_vaddr and returns true. */
bool minemu_user_load(const struct minemu_boot_info *boot_info, const char *name,
                      uint32_t *entry_vaddr);
 
/* True if every page in [vaddr, vaddr + length) is a valid user page that the
 * user may read (or write, when `write` is true). Rejects ranges that wrap or
 * leave the user region. A zero-length range is accepted. */
bool minemu_user_range_ok(uint32_t vaddr, uint32_t length, bool write);
 
void minemu_enter_user(uint32_t entry_vaddr, uint32_t stack_top) __attribute__((noreturn));
 
#endif