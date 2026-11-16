/* SPDX-License-Identifier: GPL-3.0-or-later */
#include <aelthuvor/text.h>

void ael_text_init(struct ael_text *text, struct ael_vga *vga, uint8_t attribute)
{
    *text = (struct ael_text){.vga = vga, .attribute = attribute};
}

int ael_text_cursor(struct ael_text *text)
{
    if (!text->error) { text->error = ael_vga_cursor(text->vga, text->row * VGA_COLUMNS + text->column); }
    return text->error;
}

int ael_text_clear(struct ael_text *text)
{
    if (!text->error) { text->error = ael_vga_fill(text->vga, 0, VGA_COLUMNS * VGA_ROWS, text->attribute); }
    if (!text->error) { text->row = text->column = 0; }
    return ael_text_cursor(text);
}

static void newline(struct ael_text *text)
{
    text->column = 0;
    if (++text->row == VGA_ROWS) {
        text->error = ael_vga_scroll(text->vga, text->attribute);
        text->row = VGA_ROWS - 1;
    }
}

int ael_text_write(struct ael_text *text, const char *message)
{
    while (!text->error && *message) {
        char ch = *message++;
        if (ch == '\n') { newline(text); continue; }
        text->error = ael_vga_cell(text->vga, text->row * VGA_COLUMNS + text->column, ch, text->attribute);
        if (!text->error && ++text->column == VGA_COLUMNS) { newline(text); }
    }
    return ael_text_cursor(text);
}

int ael_text_backspace(struct ael_text *text)
{
    if (text->error) { return text->error; }
    if (text->column) { --text->column; }
    else if (text->row) { --text->row; text->column = VGA_COLUMNS - 1; }
    else { return 0; }
    text->error = ael_vga_cell(text->vga, text->row * VGA_COLUMNS + text->column, ' ', text->attribute);
    return ael_text_cursor(text);
}

void ael_text_number(struct ael_text *text, uint32_t number)
{
    char digits[11];
    unsigned length = 0;
    do { digits[length++] = (char)('0' + number % 10); number /= 10; } while (number);
    while (length) {
        char one[2] = {digits[--length], 0};
        ael_text_write(text, one);
    }
}
