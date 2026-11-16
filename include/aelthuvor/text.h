/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef AELTHUVOR_TEXT_H
#define AELTHUVOR_TEXT_H
#include <aelthuvor/vga.h>
struct ael_text {
    struct ael_vga *vga;
    unsigned row, column;
    uint8_t attribute;
    int error;
};
void ael_text_init(struct ael_text *text, struct ael_vga *vga, uint8_t attribute);
int ael_text_write(struct ael_text *text, const char *message);
int ael_text_clear(struct ael_text *text);
int ael_text_backspace(struct ael_text *text);
int ael_text_cursor(struct ael_text *text);
void ael_text_number(struct ael_text *text, uint32_t number);
#endif
