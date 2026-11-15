/* SPDX-License-Identifier: GPL-3.0-or-later */
#include <aelthuvor/syscall.h>
#include <stddef.h>
int32_t ael_call(uint32_t number, uint32_t bx, uint32_t cx,
                 uint32_t dx, uint32_t si, uint32_t *extra)
{
    return liberums_call(number, bx, cx, dx, si, extra);
}
void ael_log(const char *text)
{
    while (*text != '\0') {
        if (ael_call(SYS_LOG_CHAR, (unsigned char)*text++, 0, 0, 0, NULL) != 0) {
            break; /* The kernel bounds diagnostic output to 128 characters. */
        }
    }
}
void ael_yield(void)
{
    ael_call(SYS_YIELD, 0, 0, 0, 0, NULL);
}
_Noreturn void ael_exit(uint32_t status)
{
    liberums_exit(status);
}
