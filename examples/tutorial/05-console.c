/* SPDX-License-Identifier: GPL-3.0-or-later */
#include <aelthuvor/app_console.h>
char lesson_line[64];volatile uint32_t lesson_console_stage;
__attribute__((noinline)) void lesson_console_observed(uint32_t stage)
{
    lesson_console_stage=stage;__asm__ volatile("nop" : : : "memory");
}
int ael_app_main(struct ael_app_context *ctx)
{
    if (ael_app_console_open(ctx)) { return 1; }
    if (ael_app_write(ctx,"Type a short line: ")) { return 2; }
    lesson_console_observed(0);
    int length=ael_app_readline(ctx,lesson_line,sizeof lesson_line);
    if (length==AEL_APP_EOVERFLOW) {
        (void)ael_app_console_close(ctx);return 3; /* Queue lost input; retry explicitly. */
    }
    if (length<0) { return 4; }
    lesson_console_observed(1);
    if (ael_app_write(ctx,"You typed: ") || ael_app_write(ctx,lesson_line)) { return 5; }
    if (ael_app_console_close(ctx)) { return 6; }
    lesson_console_observed(2);
    return 0;
}
