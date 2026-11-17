/* SPDX-License-Identifier: GPL-3.0-or-later */
#include <aelthuvor/app.h>
/* Application-owned observation data, not LibOS metadata. */
volatile unsigned memory_stage;
volatile uint32_t *memory_payload;
__attribute__((noinline)) void memory_observed(unsigned stage)
{
    memory_stage = stage;
    __asm__ volatile ("nop" : : : "memory");
}
int ael_app_main(struct ael_app_context *context)
{
    struct ael_app_info info;
    if (ael_app_get_info(context, &info)) { return 20; }
    void *a = ael_app_alloc(context, 64), *b = ael_app_alloc(context, 256);
    void *c = ael_app_alloc(context, 64), *d = ael_app_alloc(context, 128);
    void *e = ael_app_alloc(context, 64);
    if (!a || !b || !c || !d || !e) { return 21; }
    if (ael_app_free(context, b) || ael_app_free(context, d)) { return 22; }
    void *chosen = ael_app_alloc(context, 96);
    if (chosen != (info.policy == AEL_APP_FIRST_FIT ? b : d)) { return 23; }
    if (ael_app_free(context, chosen) || ael_app_free(context, a) || ael_app_free(context, c)
        || ael_app_free(context, e) || ael_app_trim(context)) { return 24; }
    memory_payload = ael_app_alloc(context, 4096);
    if (!memory_payload) { return 25; }
    for (unsigned i = 0; i < 1024; ++i) { memory_payload[i] = 0xa4e10000u | i; }
    memory_observed(2);
    /* Normal single-threaded code that can pause/resume at arbitrary IRQs. */
    for (volatile unsigned i = 0; i < 12000000; ++i) { __asm__ volatile ("" : : : "memory"); }
    if (ael_app_check(context)) { return 26; }
    for (unsigned i = 0; i < 1024; ++i) { if (memory_payload[i] != (0xa4e10000u | i)) { return 27; } }
    if (ael_app_free(context, (void *)memory_payload) || ael_app_trim(context)) { return 28; }
    if (ael_app_get_info(context, &info) || info.live_allocations || info.pages_owned) { return 29; }
    memory_observed(3);
    return ael_app_log(context, "Aelthuvor application memory checks passed\n") ? 30 : 0;
}
