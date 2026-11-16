/* SPDX-License-Identifier: GPL-3.0-or-later */
#define _GNU_SOURCE
#include <aelthuvor/runtime.h>
#include "memory_checks.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <unistd.h>

_Noreturn void ael_exit(uint32_t status) { exit((int)status); }

static unsigned assertions;
#define CHECK(condition) do { ++assertions; if (!(condition)) { \
    fprintf(stderr, "FAIL line %d: %s\n", __LINE__, #condition); exit(1); } } while (0)
struct mock_resource { int fd; uint32_t generation; unsigned owned; uintptr_t va; };
static struct mock_resource resources[4];
static uint32_t mock_version = LIBERUMS_ABI_VERSION, mock_features = LIBERUMS_FEATURES;
static int claim_error, map_error, release_error;
static unsigned calls;

int32_t ael_call(uint32_t number, uint32_t bx, uint32_t cx, uint32_t dx, uint32_t si, uint32_t *extra)
{
    ++calls;
    if (extra) {
        *extra = 0;
    }
    if (number == SYS_ABI_QUERY) {
        if (extra) { *extra = mock_features; }
        return (int32_t)mock_version;
    }
    if (number == SYS_RESOURCE_LIMITS) {
        if (extra) { *extra = 4; }
        return 4;
    }
    if (number == SYS_PAGE_CLAIM) {
        if (claim_error) { return claim_error; }
        for (unsigned i = 0; i < 4; ++i) {
            struct mock_resource *r = &resources[i];
            if (r->owned) { continue; }
            CHECK((bx & ~RESOURCE_ALL) == 0);
            r->fd = memfd_create("ael-test-page", 0);
            CHECK(r->fd >= 0 && ftruncate(r->fd, 4096) == 0);
            ++r->generation;
            r->owned = 1;
            if (extra) { *extra = r->generation; }
            return (int32_t)i + 1;
        }
        return RES_ENOSPC;
    }
    if (number == SYS_PAGE_UNMAP) {
        for (unsigned i = 0; i < 4; ++i) {
            if (resources[i].owned && resources[i].va == bx) {
                CHECK(munmap((void *)(uintptr_t)bx, 4096) == 0);
                resources[i].va = 0;
                return 0;
            }
        }
        return RES_ENOENT;
    }
    if (bx == 0 || bx > 4 || !resources[bx - 1].owned || resources[bx - 1].generation != cx) {
        return RES_ESTALE;
    }
    struct mock_resource *r = &resources[bx - 1];
    if (number == SYS_PAGE_MAP) {
        if (map_error) { return map_error; }
        CHECK(dx >= RESOURCE_WINDOW && dx < RESOURCE_WINDOW_END && dx % 4096 == 0);
        CHECK(si == (RESOURCE_READ | RESOURCE_WRITE));
        void *mapped = mmap((void *)(uintptr_t)dx, 4096, PROT_READ | PROT_WRITE,
                            MAP_SHARED | MAP_FIXED_NOREPLACE, r->fd, 0);
        if (mapped == MAP_FAILED) { return RES_EBUSY; }
        CHECK(r->va == 0);
        r->va = dx;
        return 0;
    }
    if (number == SYS_PAGE_RELEASE) {
        if (release_error) { return release_error; }
        if (r->va) { CHECK(munmap((void *)r->va, 4096) == 0); }
        CHECK(close(r->fd) == 0);
        r->va = r->owned = 0;
        return 0;
    }
    return -38;
}

static void failures(void)
{
    struct ael_runtime runtime = {0};
    CHECK(ael_runtime_probe(&runtime, 1, 1, AEL_REQUIRED_FEATURES) == 0);
    CHECK(runtime.quota == 4 && runtime.bindings == 4 && runtime.pages.owned == 0);
    CHECK(ael_runtime_probe(&runtime, 2, 0, AEL_REQUIRED_FEATURES) == RES_EPROTONOSUPPORT);
    CHECK(ael_runtime_probe(&runtime, 1, 3, AEL_REQUIRED_FEATURES) == RES_EPROTONOSUPPORT);
    CHECK(ael_runtime_probe(&runtime, 1, 0, 15) == 0); /* ABI 1.0 remains compatible. */
    mock_features &= ~LIBERUMS_FEATURE_PAGES;
    CHECK(ael_runtime_probe(&runtime, 1, 1, AEL_REQUIRED_FEATURES) == RES_EPROTONOSUPPORT);
    mock_features = LIBERUMS_FEATURES;
    mock_version = (uint32_t)-38;
    CHECK(ael_runtime_probe(&runtime, 1, 1, AEL_REQUIRED_FEATURES) == RES_EPROTONOSUPPORT);
    mock_version = LIBERUMS_ABI_VERSION;
    struct ael_page_pool *pool = &runtime.pages;
    struct ael_page *page = NULL;
    claim_error = RES_ENOMEM;
    CHECK(ael_page_acquire(pool, &page) == RES_ENOMEM && page == NULL && pool->owned == 0);
    claim_error = 0;
    map_error = RES_ENOMEM;
    CHECK(ael_page_acquire(pool, &page) == RES_ENOMEM && page == NULL && pool->owned == 0);
    map_error = RES_EBUSY;
    CHECK(ael_page_acquire(pool, &page) == RES_ENOSPC && page == NULL && pool->owned == 0);
    map_error = RES_ENOMEM;
    /* A failed release must retain the handle for a later retry. */
    release_error = RES_EAGAIN;
    CHECK(ael_page_acquire(pool, &page) == RES_EAGAIN && page != NULL && pool->owned == 1);
    release_error = map_error = 0;
    CHECK(ael_page_map(pool, page) == 0);
    *(uint32_t *)page->address = 0xdeadbeef;
    CHECK(ael_page_map(pool, page) == RES_EBUSY);
    CHECK(ael_page_unmap(pool, page) == 0 && ael_page_unmap(pool, page) == RES_ENOENT);
    CHECK(ael_page_map(pool, page) == 0 && *(uint32_t *)page->address == 0xdeadbeef);
    release_error = RES_EAGAIN;
    CHECK(ael_page_release(pool, page) == RES_EAGAIN && page->mapped && pool->owned == 1);
    release_error = 0;
    CHECK(ael_page_release(pool, page) == 0 && pool->owned == 0);
    CHECK(ael_page_release(pool, page) == RES_ESTALE);
    CHECK(ael_page_release(pool, (void *)(uintptr_t)1) == RES_ESTALE);
    CHECK(ael_heap_init(&runtime.heap, pool, AEL_FIRST_FIT) == 0);
    void *p = ael_alloc(&runtime.heap, 32);
    CHECK(p != NULL && ael_free(&runtime.heap, p) == 0);
    release_error = RES_EAGAIN;
    CHECK(ael_heap_trim(&runtime.heap) == RES_EAGAIN && pool->owned == 1);
    release_error = 0;
    CHECK(ael_heap_destroy(&runtime.heap) == 0 && pool->owned == 0);
}

static void randomized(enum ael_policy policy)
{
    struct ael_page_pool pool;
    struct ael_heap heap;
    CHECK(ael_pages_init(&pool, 4) == 0 && ael_heap_init(&heap, &pool, policy) == 0);
    struct allocation { unsigned char *pointer; unsigned size; unsigned char marker; } live[64] = {0};
    uint32_t seed = 0x31415926;
    for (unsigned step = 0; step < 3000; ++step) {
        seed = seed * 1664525u + 1013904223u;
        unsigned index = (seed >> 16) % 64;
        if (live[index].pointer) {
            CHECK(ael_free(&heap, live[index].pointer) == 0);
            live[index].pointer = NULL;
        } else {
            unsigned size = (seed % 1024) + 1;
            unsigned char *p = ael_alloc(&heap, size);
            if (p) {
                live[index] = (struct allocation){p, size, (unsigned char)index};
                memset(p, live[index].marker, size);
            } else {
                CHECK(heap.error == RES_ENOSPC);
            }
        }
        for (unsigned i = 0; i < 64; ++i) {
            if (!live[i].pointer) { continue; }
            for (unsigned j = 0; j < live[i].size; ++j) {
                CHECK(live[i].pointer[j] == live[i].marker);
            }
            for (unsigned j = i + 1; j < 64; ++j) {
                if (live[j].pointer) {
                    uintptr_t a = (uintptr_t)live[i].pointer, b = (uintptr_t)live[j].pointer;
                    CHECK(a + live[i].size <= b || b + live[j].size <= a);
                }
            }
        }
        if (step % 37 == 0) { CHECK(ael_heap_trim(&heap) == 0); }
    }
    for (unsigned i = 0; i < 64; ++i) {
        if (live[i].pointer) { CHECK(ael_free(&heap, live[i].pointer) == 0); }
    }
    CHECK(ael_heap_destroy(&heap) == 0 && pool.owned == 0 && heap.live == 0);
}

int main(void)
{
    failures();
    for (unsigned policy = AEL_FIRST_FIT; policy <= AEL_BEST_FIT; ++policy) {
        struct ael_page_pool pool;
        struct ael_heap heap;
        CHECK(ael_pages_init(&pool, 4) == 0 && ael_heap_init(&heap, &pool, (enum ael_policy)policy) == 0);
        CHECK(ael_memory_checks(&heap) == 0);
        randomized((enum ael_policy)policy);
        for (unsigned i = 0; i < 4; ++i) { CHECK(!resources[i].owned && !resources[i].va); }
    }
    printf("PASS: %u host assertions; both policies, ABI, rollback, page lifecycle; %u mock calls\n", assertions, calls);
    return 0;
}
