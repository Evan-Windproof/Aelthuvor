/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "console.h"

static int equal(const char *a, const char *b)
{
    while (*a && *a == *b) { ++a; ++b; }
    return *a == *b;
}

static int memtest(struct ael_console *c)
{
    unsigned before = c->heap->live;
    unsigned char *small = ael_alloc(c->heap, 96);
    unsigned char *large = ael_alloc(c->heap, 4096);
    int error = 0;
    if (!small || !large) { error = RES_ENOMEM; }
    if (!error) {
        for (unsigned i = 0; i < 96; ++i) { small[i] = (unsigned char)(i ^ 0x5a); }
        for (unsigned i = 0; i < 4096; ++i) { large[i] = (unsigned char)(i ^ 0xa5); }
        for (unsigned i = 0; i < 96; ++i) { if (small[i] != (unsigned char)(i ^ 0x5a)) { error = RES_EINVAL; } }
        for (unsigned i = 0; i < 4096; ++i) { if (large[i] != (unsigned char)(i ^ 0xa5)) { error = RES_EINVAL; } }
    }
    if (small && ael_free(c->heap, small)) { error = RES_EINVAL; }
    if (large && ael_free(c->heap, large)) { error = RES_EINVAL; }
    if (ael_heap_trim(c->heap) || c->heap->live != before || ael_heap_check(c->heap)) { error = RES_EINVAL; }
    if (!error) { ++c->memtests; }
    return error;
}

static void status(struct ael_console *c)
{
    struct ael_text *t = &c->text;
    ael_text_write(t, "ABI "); ael_text_number(t, ael_runtime.version >> 16);
    ael_text_write(t, "."); ael_text_number(t, ael_runtime.version & 0xffff);
    ael_text_write(t, " features="); ael_text_number(t, ael_runtime.features);
    ael_text_write(t, c->heap->policy == AEL_FIRST_FIT ? " first-fit" : " best-fit");
    ael_text_write(t, "\nHeap pages="); ael_text_number(t, c->heap->pool->owned);
    ael_text_write(t, " live="); ael_text_number(t, c->heap->live);
    uint32_t count, lost;
    int error = ael_keyboard_stats(&c->keyboard, &count, &lost);
    ael_text_write(t, "\nVGA mapped="); ael_text_number(t, c->vga.mapped);
    if (!error) {
        ael_text_write(t, " PS/2 queued="); ael_text_number(t, count);
        ael_text_write(t, " lost="); ael_text_number(t, lost);
    } else { ael_text_write(t, " keyboard stats unavailable"); }
    ael_text_write(t, "\n");
}

static void command(struct ael_console *c)
{
    struct ael_text *t = &c->text;
    ael_text_write(t, "\n");
    if (equal(c->line, "help")) { ael_text_write(t, "help clear about status memtest exit\n"); }
    else if (equal(c->line, "clear")) { ael_text_clear(t); }
    else if (equal(c->line, "about")) { ael_text_write(t, "Aelthuvor: independent Ring 3 Library OS\n"); }
    else if (equal(c->line, "status")) { status(c); }
    else if (equal(c->line, "memtest")) {
        int error = memtest(c);
        if (error && error != RES_ENOMEM) { ael_exit(96); }
        ael_text_write(t, error == 0 ? "memtest: PASS (96 + 4096 bytes, freed, trimmed)\n" : "memtest: FAIL (allocation)\n");
    } else if (equal(c->line, "exit")) { c->running = 0; ael_text_write(t, "Console exiting\n"); }
    else if (*c->line) { ael_text_write(t, "Unknown command\n"); }
    ++c->commands;
    c->length = 0;
    c->line[0] = 0;
    if (c->running) { ael_text_write(t, "ael> "); }
}

int ael_console_close(struct ael_console *c)
{
    c->running = 0;
    int error = 0;
    if (c->keyboard.owned) { error = ael_keyboard_release(&c->keyboard); }
    if (c->vga.owned) {
        int other = ael_vga_release(&c->vga);
        if (!error) { error = other; }
    }
    if (c->line) {
        int other = ael_free(c->heap, c->line);
        if (!error) { error = other; }
        if (!other) { c->line = NULL; c->length = 0; }
    }
    int other = ael_heap_trim(c->heap);
    return error ? error : other;
}

int ael_console_open(struct ael_console *c, struct ael_heap *heap)
{
    *c = (struct ael_console){.heap = heap};
    int error = ael_vga_acquire(&c->vga, RESOURCE_READ | RESOURCE_WRITE);
    if (!error) { error = ael_vga_map(&c->vga, VGA_USER_ADDRESS, RESOURCE_READ | RESOURCE_WRITE); }
    if (!error) { error = ael_keyboard_acquire(&c->keyboard); }
    if (!error) {
        c->line = ael_alloc(heap, AEL_CONSOLE_LINE);
        if (!c->line) { error = heap->error; }
    }
    if (error) { (void)ael_console_close(c); return error; }
    c->line[0] = 0;
    c->running = 1;
    ael_text_init(&c->text, &c->vga, 0x1f);
    ael_text_clear(&c->text);
    ael_text_write(&c->text, "Aelthuvor Phase 3 Console\nType help for commands\nael> ");
    if (c->text.error) { error = c->text.error; (void)ael_console_close(c); }
    return error;
}

int ael_console_step(struct ael_console *c, uint32_t *raw)
{
    /* Device and RAM lifetimes are checked separately; neither check pins a
     * mapping across preemption. A forced-revoke race may still hardware-fault. */
    int error = ael_heap_check(c->heap);
    if (error) { ael_exit(96); }
    error = ael_vga_check(&c->vga);
    if (error) { return error; }
    error = ael_keyboard_poll(&c->keyboard, raw);
    if (error == RES_EOVERFLOW) {
        c->length = 0; c->line[0] = 0;
        ael_text_write(&c->text, "\nInput overflow: line discarded\nael> ");
        return c->text.error ? c->text.error : error;
    }
    if (error) { return error; }
    char key = ael_key_decode(&c->keyboard.decoder, (uint8_t)*raw);
    if (key == '\n') { command(c); }
    else if (key == '\b' && c->length) {
        c->line[--c->length] = 0;
        ael_text_backspace(&c->text);
    } else if (key >= ' ' && key <= '~' && c->length + 1 < AEL_CONSOLE_LINE) {
        c->line[c->length++] = key; c->line[c->length] = 0;
        char one[2] = {key, 0};
        ael_text_write(&c->text, one);
    }
    return c->text.error;
}
