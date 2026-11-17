/* SPDX-License-Identifier: GPL-3.0-or-later */
.code32
.section .text.boot,"ax"
.global _start
.type _start,@function
_start:
    cld
    andl $-16, %esp
    call reference_start
    ud2
.size _start, .-_start
.section .note.GNU-stack,"",@progbits
