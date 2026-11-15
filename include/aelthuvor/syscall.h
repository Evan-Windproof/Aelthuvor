/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef AELTHUVOR_SYSCALL_H
#define AELTHUVOR_SYSCALL_H
#include <abi/syscall.h>
/* Only scalar registers cross into Liberums; extra points to user-local storage. */
int32_t ael_call(uint32_t number, uint32_t bx, uint32_t cx,
                 uint32_t dx, uint32_t si, uint32_t *extra);
void ael_log(const char *text);
void ael_yield(void);
_Noreturn void ael_exit(uint32_t status);
#endif
