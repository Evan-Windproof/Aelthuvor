/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef REFERENCE_POOL_H
#define REFERENCE_POOL_H
#include <abi/resource.h>
#include <stddef.h>
#define REFERENCE_OBJECT_BYTES 64u
#define REFERENCE_OBJECTS 64u
/* Object-stack policy over ONE actual protected page. No variable-size heap. */
struct reference_pool {
    struct resource_handle handle;
    uint32_t address;
    unsigned owned, mapped, available, live;
    uint8_t free_stack[REFERENCE_OBJECTS], used[REFERENCE_OBJECTS];
};
int reference_pool_open(struct reference_pool *pool);
void *reference_pool_take(struct reference_pool *pool);
int reference_pool_give(struct reference_pool *pool,void *object);
int reference_pool_close(struct reference_pool *pool);
int reference_application(struct reference_pool *pool);
#endif
