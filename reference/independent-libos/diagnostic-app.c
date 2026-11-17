/* SPDX-License-Identifier: GPL-3.0-or-later */
/* Explicit negative-test consumer, selected instead of application.c. GDB writes
 * only these unprivileged scalar inputs. The kernel still validates authority. */
#include "pool.h"
#include "syscall.h"
volatile uint32_t reference_command, reference_peer_index, reference_peer_generation, reference_peer_address;
volatile int32_t reference_query_result, reference_stale_result;
__attribute__((noinline)) void reference_observed(unsigned stage)
{
    __asm__ volatile("nop" : : "r"(stage) : "memory");
}
static void fault(uint32_t address)
{
    uint32_t value;
    __asm__ volatile(".global reference_fault_load\nreference_fault_load: movl (%1),%0" : "=r"(value) : "r"(address) : "memory");
    (void)value;
}
int reference_application(struct reference_pool *pool)
{
    struct resource_handle old=pool->handle;
    if (reference_pool_close(pool) || reference_pool_open(pool)) { return 70; }
    uint32_t extra;
    reference_stale_result=reference_call(SYS_PAGE_QUERY,old.index,old.generation,0,0,&extra);
    if (reference_stale_result!=RES_ESTALE || extra!=0) { return 71; }
    unsigned char *objects[64];
    for (unsigned i=0;i<64;++i) {
        objects[i]=reference_pool_take(pool);
        if (!objects[i]) { return 72; }
        for (unsigned j=0;j<64;++j) { objects[i][j]=(unsigned char)(i^j^0x6d); }
    }
    reference_observed(2);
    for (volatile unsigned i=0;i<12000000;++i) {
        if (reference_command==1) {
            reference_query_result=reference_call(SYS_PAGE_QUERY,reference_peer_index,reference_peer_generation,0,0,&extra);
            if (reference_query_result!=RES_EPERM || extra!=0) { return 73; }
            reference_command=0;
            reference_observed(4);
        } else if (reference_command==2) { fault(reference_peer_address); }
    }
    for (unsigned i=0;i<64;++i) {
        for (unsigned j=0;j<64;++j) { if (objects[i][j]!=(unsigned char)(i^j^0x6d)) { return 74; } }
        if (reference_pool_give(pool,objects[i])) { return 75; }
    }
    reference_observed(3);
    return 0;
}
