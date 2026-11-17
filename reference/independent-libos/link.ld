/* SPDX-License-Identifier: GPL-3.0-or-later */
OUTPUT_FORMAT(elf32-i386)
ENTRY(_start)
PHDRS {
    executable PT_LOAD FLAGS(5);
    constants PT_LOAD FLAGS(4);
    writable PT_LOAD FLAGS(6);
    contract PT_NOTE FLAGS(4);
}
SECTIONS {
    . = 0x08000000;
    .text : { KEEP(*(.text.boot)) *(.text .text.*) } :executable
    . = ALIGN(0x1000);
    .rodata : { *(.rodata .rodata.*) } :constants
    .note.liberums : { KEEP(*(.note.liberums)) } :constants :contract
    . = ALIGN(0x1000);
    .data : { *(.data .data.*) } :writable
    .bss (NOLOAD) : { *(.bss .bss.*) *(COMMON) } :writable
    ASSERT(. <= 0x08040000, "reference image exceeds public ELF profile")
    /DISCARD/ : { *(.comment) *(.note.GNU-stack) *(.eh_frame*) }
}
