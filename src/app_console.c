/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "app_internal.h"
#include <aelthuvor/app_console.h>
#include <aelthuvor/keyboard.h>
#include <aelthuvor/text.h>
/* One application context in each ELF/domain. Not a shared kernel singleton. */
static struct {
    struct ael_vga vga;
    struct ael_keyboard keyboard;
    struct ael_text text;
    struct ael_app_context *owner;
    int failure;
} console;

static int ready(struct ael_app_context *context)
{
    int error = ael_app_check(context);
    if (error) { return error; }
    return context->service == &console && console.owner == context ? ael_app_result(context, console.failure)
        : ael_app_result(context, RES_ENOENT);
}

static int service_result(struct ael_app_context *context, int error)
{
    if (error) { console.failure = error; }
    return ael_app_result(context, error);
}

int ael_app_console_close(struct ael_app_context *context)
{
    int error = ael_app_check(context);
    if (error) { return error; }
    if (context->service != &console || console.owner != context) { return ael_app_result(context, RES_ENOENT); }
    if (console.keyboard.owned) { error = ael_keyboard_release(&console.keyboard); }
    if (console.vga.owned) {
        int other = ael_vga_release(&console.vga);
        if (!error) { error = other; }
    }
    /* On failure keep the records for terminal cleanup/retry. */
    if (!error) {
        console.owner = NULL;
        context->service = NULL;
        context->close_service = NULL;
    }
    return ael_app_result(context, error ? error : console.failure);
}

int ael_app_console_open(struct ael_app_context *context)
{
    int error = ael_app_check(context);
    if (error) { return error; }
    if (context->service || console.owner) { return ael_app_result(context, RES_EBUSY); }
    console.failure = 0;
    console.vga = (struct ael_vga){0};
    console.keyboard = (struct ael_keyboard){0};
    console.owner = context;
    context->service = &console;
    context->close_service = ael_app_console_close;
    error = ael_vga_acquire(&console.vga, RESOURCE_READ | RESOURCE_WRITE);
    if (!error) { error = ael_vga_map(&console.vga, VGA_USER_ADDRESS, RESOURCE_READ | RESOURCE_WRITE); }
    if (!error) { error = ael_keyboard_acquire(&console.keyboard); }
    if (!error) {
        ael_text_init(&console.text, &console.vga, 0x1f);
        error = ael_text_clear(&console.text);
    }
    if (error) {
        (void)ael_app_console_close(context);
        return ael_app_result(context, error);
    }
    return ael_app_result(context, 0);
}

int ael_app_console_info(struct ael_app_context *context, struct ael_app_console_info *info)
{
    int error = ready(context);
    if (error) { return error; }
    if (!info) { return ael_app_result(context, RES_EINVAL); }
    uint32_t count, lost;
    error = ael_keyboard_stats(&console.keyboard, &count, &lost);
    if (!error) { *info = (struct ael_app_console_info){VGA_COLUMNS, VGA_ROWS, count, lost}; }
    return service_result(context, error);
}

int ael_app_write(struct ael_app_context *context, const char *message)
{
    int error = ready(context);
    if (error) { return error; }
    if (!message) { return ael_app_result(context, RES_EINVAL); }
    return service_result(context, ael_text_write(&console.text, message));
}

int ael_app_number(struct ael_app_context *context, uint32_t number)
{
    int error = ready(context);
    if (error) { return error; }
    ael_text_number(&console.text, number);
    return service_result(context, console.text.error);
}

int ael_app_clear(struct ael_app_context *context)
{
    int error = ready(context);
    return error ? error : service_result(context, ael_text_clear(&console.text));
}

int ael_app_readline(struct ael_app_context *context, char *buffer, size_t capacity)
{
    int error = ready(context);
    if (error) { return error; }
    if (buffer == NULL || capacity < 2 || capacity > 4096) { return ael_app_result(context, RES_EINVAL); }
    size_t length = 0;
    buffer[0] = 0;
    for (;;) {
        if (ael_app_check(context)) { return ael_app_error(context); }
        error = ael_vga_check(&console.vga);
        if (error) { return service_result(context, error); }
        uint32_t raw;
        error = ael_keyboard_poll(&console.keyboard, &raw);
        if (error == RES_EAGAIN) { continue; }
        if (error) {
            if (error == RES_EOVERFLOW) { buffer[0] = 0; }
            return error == RES_EOVERFLOW ? ael_app_result(context, error) : service_result(context, error);
        }
        char key = ael_key_decode(&console.keyboard.decoder, (uint8_t)raw);
        if (key == '\n') {
            error = ael_text_write(&console.text, "\n");
            service_result(context, error);
            return error ? error : (int)length;
        }
        if (key == '\b' && length) {
            buffer[--length] = 0;
            error = ael_text_backspace(&console.text);
        } else if (key >= ' ' && key <= '~' && length + 1 < capacity) {
            buffer[length++] = key;
            buffer[length] = 0;
            char one[2] = {key, 0};
            error = ael_text_write(&console.text, one);
        }
        if (error) { return service_result(context, error); }
    }
}
