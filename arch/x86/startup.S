/* SPDX-License-Identifier: GPL-3.0-or-later */
.section .text.entry,"ax"
.code32
.global _start
.type _start,@function
_start:
    cld
    /* The loader supplies a private zeroed stack; align the C call site. */
    andl $-16, %esp
    call ael_start
    ud2
.size _start, .-_start
.section .note.GNU-stack,"",@progbits
