/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef AELTHUVOR_CONSOLE_APP_H
#define AELTHUVOR_CONSOLE_APP_H
#include <aelthuvor/runtime.h>
#include <aelthuvor/text.h>
#include <aelthuvor/keyboard.h>
#define AEL_CONSOLE_LINE 64u
struct ael_console {
    struct ael_vga vga;
    struct ael_keyboard keyboard;
    struct ael_text text;
    struct ael_heap *heap;
    char *line; /* The input buffer is a real ael_alloc allocation, not VGA RAM. */
    unsigned length, running, commands, memtests;
};
int ael_console_open(struct ael_console *console, struct ael_heap *heap);
int ael_console_step(struct ael_console *console, uint32_t *raw);
int ael_console_close(struct ael_console *console);
#endif
