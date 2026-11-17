/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef REFERENCE_SYSCALL_H
#define REFERENCE_SYSCALL_H
#include <abi/syscall.h>
int32_t reference_call(uint32_t number,uint32_t bx,uint32_t cx,uint32_t dx,uint32_t si,uint32_t *extra);
int reference_log(const char *message);
_Noreturn void reference_exit(uint32_t status);
#endif
