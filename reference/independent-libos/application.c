/* SPDX-License-Identifier: GPL-3.0-or-later */
/* Consumer of this reference LibOS's object pool, not a kernel client. */
#include "pool.h"
#include "syscall.h"
volatile unsigned reference_stage;
__attribute__((noinline)) void reference_observed(unsigned stage)
{
    reference_stage=stage;
    __asm__ volatile("nop" : : : "memory");
}
int reference_application(struct reference_pool *pool)
{
    unsigned char *objects[REFERENCE_OBJECTS];
    for (unsigned i=0;i<REFERENCE_OBJECTS;++i) {
        objects[i]=reference_pool_take(pool);
        if (!objects[i]) { return 60; }
        for (unsigned j=0;j<REFERENCE_OBJECT_BYTES;++j) { objects[i][j]=(unsigned char)(i^j^0x6d); }
    }
    if (reference_pool_take(pool)!=NULL) { return 61; }
    reference_observed(2);
    for (volatile unsigned i=0;i<12000000;++i) { __asm__ volatile("" : : : "memory"); }
    for (unsigned i=0;i<REFERENCE_OBJECTS;++i) {
        for (unsigned j=0;j<REFERENCE_OBJECT_BYTES;++j) { if (objects[i][j]!=(unsigned char)(i^j^0x6d)) { return 62; } }
        if (reference_pool_give(pool,objects[i])) { return 63; }
    }
    if (reference_pool_give(pool,objects[0])!=RES_EINVAL) { return 64; }
    /* LIFO reuse is deliberately different from a variable-size first/best fit. */
    void *again=reference_pool_take(pool);
    if (again!=objects[REFERENCE_OBJECTS-1] || reference_pool_give(pool,again)) { return 65; }
    reference_observed(3);
    return reference_log("Independent LibOS: fixed-object pool passed\n") ? 66 : 0;
}
