/* SPDX-License-Identifier: GPL-3.0-or-later */
#include <aelthuvor/app.h>
#ifdef PROJECT_CONSOLE
#include <aelthuvor/app_console.h>
char lesson_line[64];volatile uint32_t lesson_console_stage;
__attribute__((noinline)) void lesson_console_observed(uint32_t stage)
{
    lesson_console_stage=stage;__asm__ volatile("nop" : : : "memory");
}
#endif
int ael_app_main(struct ael_app_context *ctx)
{
    uint32_t *values=ael_app_alloc(ctx,128*sizeof *values);if (!values) { return 1; }
    uint32_t total=0;
    for (unsigned i=0;i<128;++i) { values[i]=i; }
    for (unsigned i=0;i<128;++i) { total+=values[i]; }
    if (total!=8128 || ael_app_free(ctx,values) || ael_app_trim(ctx)) { return 2; }
#ifdef PROJECT_CONSOLE
    if (ael_app_console_open(ctx) || ael_app_write(ctx,"Project: enter a label: ")) { return 3; }
    lesson_console_observed(0);
    int length=ael_app_readline(ctx,lesson_line,sizeof lesson_line);
    if (length<0) { return 4; }
    lesson_console_observed(1);
    if (ael_app_write(ctx,"sum = ") || ael_app_number(ctx,total) || ael_app_write(ctx,"\n")) { return 5; }
    if (ael_app_console_close(ctx)) { return 6; }
    lesson_console_observed(2);
#endif
    return ael_app_log(ctx,"Project checksum = 8128; resources released\n") ? 7 : 0;
}
