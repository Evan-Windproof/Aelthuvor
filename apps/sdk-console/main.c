/* SPDX-License-Identifier: GPL-3.0-or-later */
#include <aelthuvor/app_console.h>
volatile unsigned sdk_console_stage, sdk_console_sequence;
char *sdk_console_line;
__attribute__((noinline)) void sdk_console_observed(unsigned stage)
{
    sdk_console_stage = stage;
    ++sdk_console_sequence;
    __asm__ volatile ("nop" : : : "memory");
}
static int equal(const char *a, const char *b)
{
    while (*a && *a == *b) { ++a; ++b; }
    return *a == *b;
}
static int memtest(struct ael_app_context *context)
{
    struct ael_app_info before, after;
    if (ael_app_get_info(context, &before)) { return AEL_APP_EINVAL; }
    unsigned char *a = ael_app_alloc(context, 96), *b = ael_app_alloc(context, 4096);
    int error = 0;
    if (!a || !b) { error = AEL_APP_ENOMEM; }
    if (!error) {
        for (unsigned i = 0; i < 96; ++i) { a[i] = (unsigned char)(i ^ 0x5a); }
        for (unsigned i = 0; i < 4096; ++i) { b[i] = (unsigned char)(i ^ 0xa5); }
        for (unsigned i = 0; i < 96; ++i) { if (a[i] != (unsigned char)(i ^ 0x5a)) { error = AEL_APP_EINVAL; } }
        for (unsigned i = 0; i < 4096; ++i) { if (b[i] != (unsigned char)(i ^ 0xa5)) { error = AEL_APP_EINVAL; } }
    }
    if (a && ael_app_free(context, a)) { error = AEL_APP_EINVAL; }
    if (b && ael_app_free(context, b)) { error = AEL_APP_EINVAL; }
    if (ael_app_trim(context) || ael_app_get_info(context, &after) || after.live_allocations != before.live_allocations) { error = AEL_APP_EINVAL; }
    return error;
}
static int status(struct ael_app_context *context)
{
    struct ael_app_info info;
    struct ael_app_console_info device;
    if (ael_app_get_info(context, &info) || ael_app_console_info(context, &device)) { return AEL_APP_EINVAL; }
    if (ael_app_write(context, "ABI ") || ael_app_number(context, info.kernel_version >> 16)
        || ael_app_write(context, ".") || ael_app_number(context, info.kernel_version & 0xffff)
        || ael_app_write(context, " API=1.0 live=") || ael_app_number(context, info.live_allocations)
        || ael_app_write(context, " queued=") || ael_app_number(context, device.queued)
        || ael_app_write(context, " lost=") || ael_app_number(context, device.lost)) { return AEL_APP_EINVAL; }
    return ael_app_write(context, "\n");
}
int ael_app_main(struct ael_app_context *context)
{
    if (ael_app_console_open(context)) { return 97; }
    char *line = ael_app_alloc(context, 64);
    if (!line) { return 97; } /* Entry cleanup closes the acquired devices. */
    if (ael_app_write(context, "Aelthuvor Application SDK Console\n")) { return 98; }
    sdk_console_line = line;
    sdk_console_observed(0);
    int status_code = 0;
    for (;;) {
        if (ael_app_write(context, "ael> ")) { status_code = 98; break; }
        int length = ael_app_readline(context, line, 64);
        if (length == AEL_APP_EOVERFLOW) {
            if (ael_app_write(context, "\nInput overflow: line discarded\n")) { status_code = 98; break; }
            continue;
        }
        if (length < 0) { status_code = 98; break; }
        sdk_console_observed(1);
        int error = 0;
        if (equal(line, "exit")) { sdk_console_observed(2); break; }
        if (equal(line, "help")) { error = ael_app_write(context, "help clear about status memtest exit\n"); }
        else if (equal(line, "clear")) { error = ael_app_clear(context); }
        else if (equal(line, "about")) { error = ael_app_write(context, "Aelthuvor: independent Ring 3 Library OS\n"); }
        else if (equal(line, "status")) { error = status(context); }
        else if (equal(line, "memtest")) {
            int checked = memtest(context);
            if (checked && checked != AEL_APP_ENOMEM) { ael_app_exit(context, 96); }
            error = ael_app_write(context, checked ? "memtest: FAIL (allocation)\n" : "memtest: PASS\n");
        } else if (*line) { error = ael_app_write(context, "Unknown command\n"); }
        if (error) { status_code = 98; break; }
        sdk_console_observed(2);
    }
    if (ael_app_free(context, line) || ael_app_trim(context)) { return 99; }
    return status_code;
}
