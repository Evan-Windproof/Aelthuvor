/* SPDX-License-Identifier: GPL-3.0-or-later */
#include <aelthuvor/runtime.h>
#include "memory_checks.h"
volatile uint32_t *ael_demo_memory;
#if defined(AEL_FAULT_BUILD) || defined(AEL_READONLY_BUILD)
extern void ael_demo_fault(void);
/* User instruction with a precise symbol for the hardware fault audit. */
#ifdef AEL_READONLY_BUILD
__asm__(".text\n.global ael_demo_fault\nael_demo_fault:\nmovb $0,0x08000000\nret\n");
#else
__asm__(".text\n.global ael_demo_fault\nael_demo_fault:\nmovl 0x08800000,%eax\nret\n");
#endif
#endif
#ifdef AEL_PREEMPTIVE_BUILD
__attribute__((noinline)) void ael_preempt_work(void)
{
    __asm__ volatile (".global ael_cpu_begin\nael_cpu_begin:" : : : "memory");
    for (volatile uint32_t i = 0; i < 12000000u; ++i) {
        __asm__ volatile ("" : : : "memory");
    }
    __asm__ volatile (".global ael_cpu_end\nael_cpu_end:" : : : "memory");
}
#endif
int ael_main(struct ael_heap *heap)
{
    int result = ael_memory_checks(heap);
    if (result != 0) {
        return result;
    }
    ael_demo_memory = ael_alloc(heap, LIBERUMS_PAGE_SIZE);
    if (ael_demo_memory == NULL) {
        return 40;
    }
    for (unsigned i = 0; i < 1024; ++i) {
        ael_demo_memory[i] = 0xa3110000u | i;
    }
    ael_checkpoint(2);
    ael_log(heap->policy == AEL_FIRST_FIT ? "Aelthuvor first-fit\n" : "Aelthuvor best-fit\n");
#ifdef AEL_PREEMPTIVE_BUILD
    /* Bounded CPU work, no yield and no system calls inside the loop. Volatile
     * progress provides an exact IRQ/resumption audit target without callbacks. */
    ael_preempt_work();
    if (ael_heap_check(heap) != 0) {
        ael_exit(96);
    }
#else
    ael_yield(); /* The independent peer runs under its own CR3. */
#endif
    for (unsigned i = 0; i < 1024; ++i) {
        if (ael_demo_memory[i] != (0xa3110000u | i)) {
            return 41;
        }
    }
#if defined(AEL_FAULT_BUILD) || defined(AEL_READONLY_BUILD)
    ael_demo_fault(); /* Must terminate only this domain, reclaiming the live page. */
#endif
#ifdef AEL_EXIT_LIVE_BUILD
    ael_exit(0); /* Intentionally bypass LibOS cleanup: kernel teardown must work. */
#endif
    if (ael_free(heap, (void *)ael_demo_memory) != 0 || ael_heap_trim(heap) != 0) {
        return 42;
    }
    ael_checkpoint(3);
    ael_log("Aelthuvor memory checks passed\n");
    return 0;
}
