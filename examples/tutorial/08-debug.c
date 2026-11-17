/* SPDX-License-Identifier: GPL-3.0-or-later */
#include <aelthuvor/app.h>
volatile uint32_t lesson_stage;uint32_t *lesson_pointer;
__attribute__((noinline)) void lesson_observed(uint32_t stage)
{
    lesson_stage=stage;__asm__ volatile("nop" : : : "memory");
}
int ael_app_main(struct ael_app_context *ctx)
{
    lesson_pointer=ael_app_alloc(ctx,4096);if (!lesson_pointer) { return 1; }
    for (unsigned i=0;i<1024;++i) { lesson_pointer[i]=0xa6e10000u|i; }
    lesson_observed(1);
    for (unsigned i=0;i<1024;++i) { if (lesson_pointer[i]!=(0xa6e10000u|i)) { return 2; } }
    if (ael_app_free(ctx,lesson_pointer) || ael_app_trim(ctx)) { return 3; }
    lesson_pointer=0;lesson_observed(2);
    return 0;
}
