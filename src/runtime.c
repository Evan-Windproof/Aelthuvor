/* SPDX-License-Identifier: GPL-3.0-or-later */
#include <aelthuvor/runtime.h>
#include <abi/elf.h>
#ifndef AEL_REQUIRED_MAJOR
#define AEL_REQUIRED_MAJOR LIBERUMS_ABI_MAJOR
#endif
const struct liberums_abi_note ael_requirement
    __attribute__((section(".note.liberums"), aligned(4), used)) = {
        9, 12, LIBERUMS_NOTE_TYPE, "LIBERUMS", LIBERUMS_ABI_MAJOR, LIBERUMS_ABI_MINOR,
        AEL_REQUIRED_FEATURES
    };
struct ael_runtime ael_runtime;

__attribute__((noinline)) void ael_checkpoint(uint32_t stage)
{
    __asm__ volatile ("nop" : : "r"(stage) : "memory");
}

_Noreturn void ael_start(void)
{
    if (ael_runtime_probe(&ael_runtime, AEL_REQUIRED_MAJOR, LIBERUMS_ABI_MINOR, AEL_REQUIRED_FEATURES) != 0) {
        ael_exit(93);
    }
#ifdef AEL_BEST_FIT_BUILD
    enum ael_policy policy = AEL_BEST_FIT;
#else
    enum ael_policy policy = AEL_FIRST_FIT;
#endif
    if (ael_heap_init(&ael_runtime.heap, &ael_runtime.pages, policy) != 0) {
        ael_exit(94);
    }
    ael_checkpoint(1);
    int status = ael_main(&ael_runtime.heap);
    if (status == 0 && ael_heap_destroy(&ael_runtime.heap) != 0) {
        status = 95;
    }
    ael_exit((uint32_t)status);
}
