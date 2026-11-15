/* SPDX-License-Identifier: GPL-3.0-or-later */
#include <aelthuvor/memory.h>

int ael_heap_init(struct ael_heap *heap, struct ael_page_pool *pool, enum ael_policy policy)
{
    if (heap == NULL || pool == NULL || pool->quota == 0
        || (policy != AEL_FIRST_FIT && policy != AEL_BEST_FIT)) {
        return RES_EINVAL;
    }
    *heap = (struct ael_heap){.pool = pool, .policy = policy};
    return 0;
}

static void *use_block(struct ael_heap *heap, struct ael_arena *arena, unsigned index, unsigned size)
{
    struct ael_block *block = &arena->blocks[index];
    if (block->size > size) {
        /* Minimum block size is 16; at most 256 blocks can partition a page.
         * There is room for a split whenever a non-minimal free block exists. */
        for (unsigned i = arena->count; i > index + 1; --i) {
            arena->blocks[i] = arena->blocks[i - 1];
        }
        arena->blocks[index + 1] = (struct ael_block){
            .offset = (uint16_t)(block->offset + size), .size = (uint16_t)(block->size - size)
        };
        ++arena->count;
        block->size = (uint16_t)size;
    }
    block->used = 1;
    ++heap->live;
    return (void *)(arena->page->address + block->offset);
}

void *ael_alloc(struct ael_heap *heap, size_t bytes)
{
    if (heap == NULL || heap->pool == NULL) {
        return NULL;
    }
    heap->error = 0;
    /* Check before rounding, avoiding size_t overflow even on i686. */
    if (bytes == 0 || bytes > LIBERUMS_PAGE_SIZE) {
        heap->error = RES_EINVAL;
        return NULL;
    }
    unsigned size = ((unsigned)bytes + AEL_ALIGNMENT - 1) & ~(AEL_ALIGNMENT - 1);
    struct ael_arena *selected = NULL;
    unsigned selected_index = 0, smallest = LIBERUMS_PAGE_SIZE + 1;
    for (unsigned i = 0; i < AEL_PAGE_SLOTS; ++i) {
        struct ael_arena *arena = &heap->arenas[i];
        if (arena->page == NULL) {
            continue;
        }
        for (unsigned j = 0; j < arena->count; ++j) {
            struct ael_block *block = &arena->blocks[j];
            if (!block->used && block->size >= size && block->size < smallest) {
                selected = arena;
                selected_index = j;
                smallest = block->size;
                if (heap->policy == AEL_FIRST_FIT) {
                    return use_block(heap, selected, selected_index, size);
                }
            }
        }
    }
    if (selected != NULL) {
        return use_block(heap, selected, selected_index, size);
    }
    /* Both policies reuse existing space before obtaining another resource. */
    for (unsigned i = 0; i < AEL_PAGE_SLOTS; ++i) {
        struct ael_arena *arena = &heap->arenas[i];
        if (arena->page != NULL) {
            continue;
        }
        struct ael_page *page;
        heap->error = ael_page_acquire(heap->pool, &page);
        if (heap->error != 0) {
            return NULL;
        }
        arena->page = page;
        arena->count = 1;
        arena->blocks[0] = (struct ael_block){.size = LIBERUMS_PAGE_SIZE};
        return use_block(heap, arena, 0, size);
    }
    heap->error = RES_ENOSPC;
    return NULL;
}

static void remove_block(struct ael_arena *arena, unsigned index)
{
    for (unsigned i = index; i + 1 < arena->count; ++i) {
        arena->blocks[i] = arena->blocks[i + 1];
    }
    --arena->count;
}

int ael_free(struct ael_heap *heap, void *pointer)
{
    if (heap == NULL || heap->pool == NULL) {
        return RES_EINVAL;
    }
    if (pointer == NULL) {
        return 0;
    }
    /* Compare integer addresses, never dereference an unvalidated pointer or
     * subtract unrelated C pointers. Only exact live block starts are accepted. */
    uintptr_t address = (uintptr_t)pointer;
    for (unsigned i = 0; i < AEL_PAGE_SLOTS; ++i) {
        struct ael_arena *arena = &heap->arenas[i];
        if (arena->page == NULL || address < arena->page->address
            || address - arena->page->address >= LIBERUMS_PAGE_SIZE) {
            continue;
        }
        unsigned offset = (unsigned)(address - arena->page->address);
        for (unsigned j = 0; j < arena->count; ++j) {
            struct ael_block *block = &arena->blocks[j];
            if (block->offset != offset || !block->used) {
                continue;
            }
            block->used = 0;
            --heap->live;
            if (j + 1 < arena->count && !arena->blocks[j + 1].used) {
                block->size = (uint16_t)(block->size + arena->blocks[j + 1].size);
                remove_block(arena, j + 1);
            }
            if (j > 0 && !arena->blocks[j - 1].used) {
                arena->blocks[j - 1].size = (uint16_t)(arena->blocks[j - 1].size + block->size);
                remove_block(arena, j);
            }
            return 0;
        }
        return RES_EINVAL;
    }
    return RES_EINVAL;
}

int ael_heap_trim(struct ael_heap *heap)
{
    if (heap == NULL || heap->pool == NULL) {
        return RES_EINVAL;
    }
    for (unsigned i = 0; i < AEL_PAGE_SLOTS; ++i) {
        struct ael_arena *arena = &heap->arenas[i];
        if (arena->page != NULL && arena->count == 1 && !arena->blocks[0].used) {
            int error = ael_page_release(heap->pool, arena->page);
            if (error != 0) {
                return error;
            }
            *arena = (struct ael_arena){0};
        }
    }
    return 0;
}

int ael_heap_destroy(struct ael_heap *heap)
{
    if (heap == NULL || heap->pool == NULL) {
        return RES_EINVAL;
    }
    if (heap->live != 0) {
        return RES_EBUSY;
    }
    return ael_heap_trim(heap);
}
