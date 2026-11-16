/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef AELTHUVOR_VGA_H
#define AELTHUVOR_VGA_H
#include <aelthuvor/syscall.h>
#include <abi/vga.h>
/* Initialize to zero once, never reinitialize a live device. Not a RAM page. */
struct ael_vga {
    struct resource_handle handle;
    uint32_t rights, cursor;
    volatile uint16_t *cells;
    unsigned owned, mapped;
    int failure;
};
int ael_vga_probe(void);
int ael_vga_acquire(struct ael_vga *vga, uint32_t rights);
int ael_vga_map(struct ael_vga *vga, uintptr_t address, uint32_t rights);
int ael_vga_release(struct ael_vga *vga);
int ael_vga_cursor(struct ael_vga *vga, unsigned cell);
int ael_vga_check(struct ael_vga *vga);
int ael_vga_cell(struct ael_vga *vga, unsigned cell, char ch, uint8_t attribute);
int ael_vga_fill(struct ael_vga *vga, unsigned first, unsigned count, uint8_t attribute);
int ael_vga_scroll(struct ael_vga *vga, uint8_t attribute);
#endif
