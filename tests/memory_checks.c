/* SPDX-License-Identifier: GPL-3.0-or-later */
/* The same behavioral checks run against a mock backend and the real kernel. */
#include "memory_checks.h"
#define CHECK(condition, code) do { if (!(condition)) { return (code); } } while (0)
int ael_memory_checks(struct ael_heap *heap)
{
    CHECK(ael_alloc(heap, 0) == NULL && heap->error == RES_EINVAL, 10);
    CHECK(ael_alloc(heap, (size_t)-1) == NULL && heap->error == RES_EINVAL, 11);
    CHECK(ael_free(heap, NULL) == 0, 12);
    CHECK(ael_free(heap, (void *)(uintptr_t)0x1234) == RES_EINVAL, 13);
    void *a = ael_alloc(heap, 64);
    void *b = ael_alloc(heap, 256);
    void *c = ael_alloc(heap, 64);
    void *d = ael_alloc(heap, 128);
    void *e = ael_alloc(heap, 64);
    CHECK(a && b && c && d && e && heap->pool->owned == 1, 14);
    CHECK(((uintptr_t)a & 15) == 0, 15);
    CHECK(ael_heap_destroy(heap) == RES_EBUSY, 16);
    CHECK(ael_free(heap, (unsigned char *)b + 1) == RES_EINVAL, 17);
    CHECK(ael_free(heap, b) == 0 && ael_free(heap, b) == RES_EINVAL, 18);
    CHECK(ael_free(heap, d) == 0, 19);
    void *chosen = ael_alloc(heap, 96);
    /* With identical kernel calls/data, policies pick different holes. */
    CHECK(chosen == (heap->policy == AEL_FIRST_FIT ? b : d), 20);
    CHECK(ael_free(heap, chosen) == 0, 21);
    CHECK(ael_free(heap, a) == 0 && ael_free(heap, c) == 0 && ael_free(heap, e) == 0, 22);
    CHECK(ael_alloc(heap, 4096) == a, 23); /* Adjacent holes coalesce. */
    CHECK(ael_free(heap, a) == 0 && ael_heap_trim(heap) == 0 && heap->pool->owned == 0, 24);
    void *pages[AEL_PAGE_SLOTS];
    for (unsigned i = 0; i < heap->pool->quota; ++i) {
        pages[i] = ael_alloc(heap, 4096);
        CHECK(pages[i] != NULL, 25);
        volatile unsigned char *bytes = pages[i];
        for (unsigned j = 0; j < 4096; ++j) {
            CHECK(bytes[j] == 0, 26);
            bytes[j] = (unsigned char)(i + j);
        }
    }
    CHECK(ael_alloc(heap, 16) == NULL && heap->error == RES_ENOSPC, 27);
    for (unsigned i = 0; i < heap->pool->quota; ++i) {
        volatile unsigned char *bytes = pages[i];
        for (unsigned j = 0; j < 4096; ++j) {
            CHECK(bytes[j] == (unsigned char)(i + j), 28);
        }
        CHECK(ael_free(heap, pages[i]) == 0, 29);
    }
    CHECK(ael_heap_destroy(heap) == 0 && heap->pool->owned == 0, 30);
    CHECK(ael_free(heap, a) == RES_EINVAL, 31);
    /* Exercise the metadata capacity with the smallest possible blocks. */
    void *small[AEL_BLOCKS_PER_PAGE];
    for (unsigned i = 0; i < AEL_BLOCKS_PER_PAGE; ++i) {
        small[i] = ael_alloc(heap, 1);
        CHECK(small[i] != NULL, 32);
        *(unsigned char *)small[i] = (unsigned char)i;
    }
    for (unsigned i = 0; i < AEL_BLOCKS_PER_PAGE; ++i) {
        CHECK(*(unsigned char *)small[i] == (unsigned char)i, 33);
        CHECK(ael_free(heap, small[i]) == 0, 34);
    }
    CHECK(ael_heap_destroy(heap) == 0 && heap->live == 0, 35);
    return 0;
}
