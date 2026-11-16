/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef AELTHUVOR_MEMORY_H
#define AELTHUVOR_MEMORY_H
#include <aelthuvor/page.h>
#define AEL_ALIGNMENT 16u
#define AEL_BLOCKS_PER_PAGE (LIBERUMS_PAGE_SIZE / AEL_ALIGNMENT)
enum ael_policy { AEL_FIRST_FIT, AEL_BEST_FIT };
/* Metadata stays outside application payload; offsets are relative to a page. */
struct ael_block { uint16_t offset, size; unsigned used; };
struct ael_arena {
    struct ael_page *page;
    unsigned count;
    struct ael_block blocks[AEL_BLOCKS_PER_PAGE];
};
struct ael_heap {
    struct ael_page_pool *pool;
    enum ael_policy policy;
    int error;
    unsigned live;
    int failure; /* Sticky: no API may return a pointer after uncertain/lost state. */
    struct ael_arena arenas[AEL_PAGE_SLOTS];
};
/* Explicit checkpoint returns an error for diagnostics. Public allocator APIs
 * terminate the domain (status 96) on a preemptive checkpoint failure. */
int ael_heap_check(struct ael_heap *heap);
int ael_heap_init(struct ael_heap *heap, struct ael_page_pool *pool, enum ael_policy policy);
void *ael_alloc(struct ael_heap *heap, size_t bytes);
int ael_free(struct ael_heap *heap, void *pointer);
int ael_heap_trim(struct ael_heap *heap);
int ael_heap_destroy(struct ael_heap *heap);
#endif
