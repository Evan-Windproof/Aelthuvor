/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "syscall.h"
int32_t reference_call(uint32_t number,uint32_t bx,uint32_t cx,uint32_t dx,uint32_t si,uint32_t *extra)
{
    __asm__ volatile("int $0x80" : "+a"(number), "+d"(dx)
                     : "b"(bx), "c"(cx), "S"(si) : "memory", "cc");
    if (extra) { *extra=dx; }
    return (int32_t)number;
}
int reference_log(const char *message)
{
    while (*message) {
        int error=reference_call(SYS_LOG_CHAR,(uint8_t)*message++,0,0,0,0);
        if (error) { return error; }
    }
    return 0;
}
_Noreturn void reference_exit(uint32_t status)
{
    reference_call(SYS_EXIT,status,0,0,0,0);
    for (;;) { __asm__ volatile("ud2"); }
}
