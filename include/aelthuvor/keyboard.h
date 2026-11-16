/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef AELTHUVOR_KEYBOARD_H
#define AELTHUVOR_KEYBOARD_H
#include <aelthuvor/syscall.h>
#include <abi/keyboard.h>
struct ael_key_decoder { unsigned shifts, caps, caps_down, extended, pause; };
struct ael_keyboard {
    struct resource_handle handle;
    unsigned owned;
    int failure;
    struct ael_key_decoder decoder;
};
int ael_keyboard_probe(void);
int ael_keyboard_acquire(struct ael_keyboard *keyboard);
int ael_keyboard_poll(struct ael_keyboard *keyboard, uint32_t *raw);
int ael_keyboard_stats(struct ael_keyboard *keyboard, uint32_t *count, uint32_t *lost);
int ael_keyboard_release(struct ael_keyboard *keyboard);
void ael_key_reset(struct ael_key_decoder *decoder);
/* US translated set 1: printable ASCII, '\b', '\n', or zero for ignored input. */
char ael_key_decode(struct ael_key_decoder *decoder, uint8_t raw);
#endif
