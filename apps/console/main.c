/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "console.h"
struct ael_console ael_console;
volatile uint32_t ael_console_sequence, ael_console_raw;
volatile int32_t ael_console_error;
#ifdef AEL_CONSOLE_DIAGNOSTIC
volatile uint32_t ael_console_test_request;
#endif
/* Observation only: neither grants devices nor alters kernel input queues. */
__attribute__((noinline)) void ael_console_observed(void)
{
    __asm__ volatile ("nop" : : : "memory");
}
int ael_main(struct ael_heap *heap)
{
    if (ael_console_open(&ael_console, heap)) { return 97; }
    ael_console_sequence = 1;
    ael_console_observed();
    int status = 0;
    while (ael_console.running) {
#ifdef AEL_CONSOLE_DIAGNOSTIC
        if (ael_console_test_request == 2) { __asm__ volatile ("ud2"); }
#endif
        uint32_t raw;
        int error = ael_console_step(&ael_console, &raw);
        if (error == RES_EAGAIN) { continue; }
        if (error && error != RES_EOVERFLOW) {
            ael_console_error = error;
            ++ael_console_sequence;
            ael_console_observed();
            status = 98;
            break;
        }
        ael_console_raw = raw;
        ++ael_console_sequence;
        ael_console_observed();
    }
    int cleanup = ael_console_close(&ael_console);
    return status ? status : (cleanup ? 99 : 0);
}
