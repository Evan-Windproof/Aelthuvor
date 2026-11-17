/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "pool.h"
#include "syscall.h"

static void check(struct reference_pool *pool)
{
    if (!pool->owned || !pool->mapped
        || reference_call(SYS_PAGE_QUERY,pool->handle.index,pool->handle.generation,0,0,0)<0) {
        reference_exit(86); /* No recovery/pinning guarantee for lost payload. */
    }
}
int reference_pool_open(struct reference_pool *pool)
{
    /* A fresh static-zero pool only. Metadata stays outside revocable memory. */
    if (pool->owned) { return RES_EBUSY; }
    uint32_t generation=0;
    int32_t index=reference_call(SYS_PAGE_CLAIM,RESOURCE_READ|RESOURCE_WRITE|RESOURCE_MAP|RESOURCE_RELEASE|RESOURCE_QUERY,0,0,0,&generation);
    if (index<0) { return index; }
    pool->handle=(struct resource_handle){(uint32_t)index,generation};
    pool->owned=1;
    pool->address=RESOURCE_WINDOW_END-LIBERUMS_PAGE_SIZE; /* Last VA, by policy. */
    int error=reference_call(SYS_PAGE_MAP,pool->handle.index,generation,pool->address,RESOURCE_READ|RESOURCE_WRITE,0);
    if (error) {
        int cleanup=reference_call(SYS_PAGE_RELEASE,pool->handle.index,generation,0,0,0);
        if (!cleanup) { pool->owned=0; }
        return cleanup ? cleanup : error; /* Retain failed cleanup identity. */
    }
    pool->mapped=1;
    pool->live=0;
    pool->available=REFERENCE_OBJECTS;
    for (unsigned i=0;i<REFERENCE_OBJECTS;++i) { pool->free_stack[i]=(uint8_t)i; pool->used[i]=0; }
    return 0;
}
void *reference_pool_take(struct reference_pool *pool)
{
    check(pool);
    if (!pool->available) { return NULL; }
    unsigned slot=pool->free_stack[--pool->available];
    pool->used[slot]=1;
    ++pool->live;
    return (void *)(uintptr_t)(pool->address+slot*REFERENCE_OBJECT_BYTES);
}
int reference_pool_give(struct reference_pool *pool,void *object)
{
    check(pool);
    uintptr_t address=(uintptr_t)object;
    if (address<pool->address || address-pool->address>=LIBERUMS_PAGE_SIZE
        || (address-pool->address)%REFERENCE_OBJECT_BYTES) { return RES_EINVAL; }
    unsigned slot=(unsigned)(address-pool->address)/REFERENCE_OBJECT_BYTES;
    if (!pool->used[slot]) { return RES_EINVAL; }
    pool->used[slot]=0;
    pool->free_stack[pool->available++]=(uint8_t)slot;
    --pool->live;
    return 0;
}
int reference_pool_close(struct reference_pool *pool)
{
    if (!pool->owned) { return RES_ENOENT; }
    if (pool->live) { return RES_EBUSY; }
    int error=reference_call(SYS_PAGE_RELEASE,pool->handle.index,pool->handle.generation,0,0,0);
    if (!error) { pool->owned=pool->mapped=0; pool->address=0; }
    return error;
}
