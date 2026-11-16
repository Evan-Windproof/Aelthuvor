/* SPDX-License-Identifier: GPL-3.0-or-later */
#define _GNU_SOURCE
#include <aelthuvor/revoke.h>
#include <aelthuvor/runtime.h>
#include <assert.h>
#include <setjmp.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>

static struct ael_page_pool pool;
static struct ael_heap heap;
static struct { unsigned owned; uint32_t generation; uintptr_t va; } resources[4];
static struct ael_revoke_event events[REVOKE_EVENTS_PER_DOMAIN];
static uint32_t version, features;
static int poll_error, info_error, ack_error, release_error, map_error;
static unsigned ack_calls, release_calls, query_calls, polls;
static unsigned retain_event, exit_expected;
static jmp_buf exit_target;
static unsigned checks;
#define CHECK(x) do { ++checks; if (!(x)) { fprintf(stderr,"FAIL line %d: %s\n",__LINE__,#x); abort(); } } while (0)

_Noreturn void ael_exit(uint32_t status)
{
    CHECK(exit_expected && status == 96);
    longjmp(exit_target, 1);
}

static void reclaim(unsigned i)
{
    if (resources[i].va) {
        CHECK(munmap((void *)resources[i].va, 4096) == 0);
    }
    resources[i].va = resources[i].owned = 0;
    for (unsigned j = 0; j < REVOKE_EVENTS_PER_DOMAIN; ++j) {
        if (events[j].handle.index == i+1) {
            events[j] = (struct ael_revoke_event){0};
        }
    }
}

int32_t ael_call(uint32_t n, uint32_t bx, uint32_t cx, uint32_t dx, uint32_t si, uint32_t *extra)
{
    if (extra) { *extra = 0; }
    if (n == SYS_ABI_QUERY) { if (extra) { *extra = features; } return (int32_t)version; }
    if (n == SYS_RESOURCE_LIMITS) { if (extra) { *extra = 4; } return 4; }
    if (n == SYS_REVOKE_POLL) {
        ++polls;
        if (poll_error) { return poll_error; }
        for (unsigned i = 0; i < REVOKE_EVENTS_PER_DOMAIN; ++i) {
            if (events[i].request) { *extra = events[i].request; return 0; }
        }
        return RES_ENOENT;
    }
    if (n == SYS_REVOKE_INFO) {
        if (info_error) { return info_error; }
        for (unsigned i = 0; i < REVOKE_EVENTS_PER_DOMAIN; ++i) {
            struct ael_revoke_event *e = &events[i];
            if (e->request == bx) {
                if (cx == REVOKE_INFO_INDEX) { *extra = e->handle.index; }
                else if (cx == REVOKE_INFO_GENERATION) { *extra = e->handle.generation; }
                else if (cx == REVOKE_INFO_REASON) { *extra = e->reason; }
                else if (cx == REVOKE_INFO_DEADLINE) { *extra = e->deadline; }
                else { return RES_EINVAL; }
                return 0;
            }
        }
        return RES_ESTALE;
    }
    if (n == SYS_REVOKE_ACK) {
        ++ack_calls;
        if (ack_error) { return ack_error; }
        for (unsigned i = 0; i < REVOKE_EVENTS_PER_DOMAIN; ++i) {
            struct ael_revoke_event *e = &events[i];
            if (e->request == dx && e->handle.index == bx && e->handle.generation == cx) {
                if (!retain_event) { reclaim(bx-1); }
                return 0;
            }
        }
        return RES_ESTALE;
    }
    if (n == SYS_PAGE_CLAIM) {
        for (unsigned i = 0; i < 4; ++i) {
            if (!resources[i].owned) {
                resources[i].owned = 1;
                *extra = ++resources[i].generation;
                return (int)i+1;
            }
        }
        return RES_ENOSPC;
    }
    if (!bx || bx > 4 || !resources[bx-1].owned || resources[bx-1].generation != cx) {
        return RES_ESTALE;
    }
    unsigned i = bx-1;
    if (n == SYS_PAGE_QUERY) { ++query_calls; return 0x100+(int)i; }
    if (n == SYS_PAGE_MAP) {
        if (map_error) { return map_error; }
        CHECK(si == (RESOURCE_READ|RESOURCE_WRITE));
        void *p = mmap((void *)(uintptr_t)dx,4096,PROT_READ|PROT_WRITE,
                       MAP_PRIVATE|MAP_ANONYMOUS|MAP_FIXED_NOREPLACE,-1,0);
        if (p == MAP_FAILED) { return RES_EBUSY; }
        resources[i].va = dx;
        return 0;
    }
    if (n == SYS_PAGE_RELEASE) {
        ++release_calls;
        if (release_error) { return release_error; }
        reclaim(i);
        return 0;
    }
    return -38;
}

static void reset(enum ael_policy policy)
{
    for (unsigned i = 0; i < 4; ++i) { reclaim(i); resources[i].generation = 0x80000000u; }
    version = 0x10002; features = LIBERUMS_PREEMPT_FEATURES;
    poll_error = info_error = ack_error = release_error = map_error = 0;
    ack_calls = release_calls = query_calls = polls = retain_event = exit_expected = 0;
    CHECK(ael_pages_init(&pool,4) == 0);
    pool.revocation = 1;
    CHECK(ael_heap_init(&heap,&pool,policy) == 0);
}

static void request(unsigned slot, struct ael_page *page, uint32_t id)
{
    CHECK(slot < REVOKE_EVENTS_PER_DOMAIN);
    events[slot] = (struct ael_revoke_event){id,page->handle,REVOKE_REASON_PRESSURE,0xffffffffu};
}

static void abi(void)
{
    struct ael_runtime rt;
    reset(AEL_FIRST_FIT);
    CHECK(ael_runtime_probe(&rt,1,2,AEL_PREEMPTIVE_FEATURES) == 0 && rt.pages.revocation);
    CHECK(rt.version == 0x10002 && rt.features == 0x77);
    CHECK(ael_runtime_probe(&rt,1,2,AEL_COOPERATIVE_FEATURES) == RES_EPROTONOSUPPORT);
    features &= ~LIBERUMS_FEATURE_REVOCATION;
    CHECK(ael_runtime_probe(&rt,1,2,AEL_PREEMPTIVE_FEATURES) == RES_EPROTONOSUPPORT);
    features = LIBERUMS_FEATURES;
    CHECK(ael_runtime_probe(&rt,1,2,AEL_COOPERATIVE_FEATURES) == 0 && !rt.pages.revocation);
    CHECK(ael_runtime_probe(&rt,1,2,AEL_PREEMPTIVE_FEATURES) == RES_EPROTONOSUPPORT);
    version = 0x10001;
    CHECK(ael_runtime_probe(&rt,1,1,AEL_COOPERATIVE_FEATURES) == 0);
    CHECK(ael_runtime_probe(&rt,1,2,AEL_COOPERATIVE_FEATURES) == RES_EPROTONOSUPPORT);
    version = 0x20002;
    CHECK(ael_runtime_probe(&rt,1,2,AEL_COOPERATIVE_FEATURES) == RES_EPROTONOSUPPORT);
}

static void scenarios(enum ael_policy policy)
{
    reset(policy);
    CHECK(ael_heap_check(&heap) == 0 && polls == 1);
    void *a = ael_alloc(&heap,64), *b = ael_alloc(&heap,256), *c = ael_alloc(&heap,64);
    void *d = ael_alloc(&heap,128), *e = ael_alloc(&heap,64);
    CHECK(a && b && c && d && e);
    CHECK(ael_free(&heap,b) == 0 && ael_free(&heap,d) == 0);
    CHECK(ael_alloc(&heap,96) == (policy == AEL_FIRST_FIT ? b : d));

    reset(policy);
    a = ael_alloc(&heap,32); CHECK(a && ael_free(&heap,a) == 0);
    struct ael_page *p = heap.arenas[0].page;
    struct resource_handle old = p->handle;
    request(0,p,0x80000001u);
    struct ael_revoke_event event;
    CHECK(ael_revoke_poll(&event) == 0 && event.request == 0x80000001u);
    CHECK(event.handle.generation == 0x80000001u && event.deadline == 0xffffffffu);
    CHECK(ael_heap_check(&heap) == 0 && ack_calls == 1 && pool.owned == 0);
    CHECK(p->state == AEL_PAGE_ACKED && !p->mapped && heap.arenas[0].page == NULL);
    CHECK(ael_heap_check(&heap) == 0 && ack_calls == 1);
    CHECK(ael_revoke_ack(&event) == RES_ESTALE); /* raw duplicate, never repeated by service */
    a = ael_alloc(&heap,32); CHECK(a && p->handle.generation != old.generation);
    CHECK(ael_free(&heap,a) == 0 && ael_heap_destroy(&heap) == 0);

    reset(policy);
    a = ael_alloc(&heap,32); p = heap.arenas[0].page;
    request(0,p,1);
    CHECK(ael_heap_check(&heap) == RES_EBUSY && ack_calls == 0);
    CHECK(p->state == AEL_PAGE_PENDING && heap.live == 1);
    unsigned before = polls;
    CHECK(ael_heap_check(&heap) == RES_EBUSY && polls == before);
    exit_expected = 1;
    if (setjmp(exit_target) == 0) { (void)ael_alloc(&heap,32); CHECK(0); }
    CHECK(ack_calls == 0); /* Public entry cannot hand control back to application. */
    if (setjmp(exit_target) == 0) { (void)ael_free(&heap,NULL); CHECK(0); }
    if (setjmp(exit_target) == 0) { (void)ael_heap_trim(&heap); CHECK(0); }
    if (setjmp(exit_target) == 0) { (void)ael_heap_destroy(&heap); CHECK(0); }
    CHECK(ack_calls == 0 && release_calls == 0);

    reset(policy);
    a = ael_alloc(&heap,32); p = heap.arenas[0].page;
    reclaim(p->handle.index-1); /* Force completion removed event, live pointer lost. */
    CHECK(ael_heap_check(&heap) == RES_ESTALE && heap.live == 1);
    CHECK(p->state == AEL_PAGE_LOST && pool.owned == 0 && ack_calls == 0);

    reset(policy);
    a = ael_alloc(&heap,32); CHECK(ael_free(&heap,a) == 0); p = heap.arenas[0].page;
    reclaim(p->handle.index-1); /* Lost idle arena can be safely detached. */
    CHECK(ael_heap_check(&heap) == 0 && heap.arenas[0].page == NULL);
    CHECK(ael_heap_destroy(&heap) == 0 && release_calls == 0);

    reset(policy);
    a = ael_alloc(&heap,32); CHECK(ael_free(&heap,a) == 0); p = heap.arenas[0].page;
    request(0,p,2); events[0].handle.generation--;
    CHECK(ael_heap_check(&heap) == RES_ESTALE && p->state == AEL_PAGE_MAPPED && ack_calls == 0);

    reset(policy);
    a = ael_alloc(&heap,32); CHECK(ael_free(&heap,a) == 0); p = heap.arenas[0].page;
    request(0,p,3); ack_error = RES_ESTALE;
    CHECK(ael_heap_check(&heap) == RES_ESTALE && p->state == AEL_PAGE_PENDING);
    CHECK(ael_heap_check(&heap) == RES_ESTALE && ack_calls == 1);

    reset(policy);
    a = ael_alloc(&heap,32); CHECK(ael_free(&heap,a) == 0); p = heap.arenas[0].page;
    request(0,p,4); info_error = RES_ESTALE;
    CHECK(ael_heap_check(&heap) == RES_ESTALE && ack_calls == 0);

    reset(policy);
    a = ael_alloc(&heap,32); CHECK(ael_free(&heap,a) == 0); p = heap.arenas[0].page;
    request(0,p,5); retain_event = 1;
    before = polls;
    CHECK(ael_heap_check(&heap) == RES_EBUSY && polls == before+2 && ack_calls == 1);

    reset(policy);
    struct ael_page *q;
    CHECK(ael_page_acquire(&pool,&p) == 0 && ael_page_acquire(&pool,&q) == 0);
    request(0,p,6); request(1,q,7);
    CHECK(ael_heap_check(&heap) == 0 && ack_calls == 2 && pool.owned == 0);

    reset(policy);
    CHECK(ael_page_acquire(&pool,&p) == 0); request(0,p,8);
    CHECK(ael_page_release(&pool,p) == 0); /* Release cancels event. */
    CHECK(ael_heap_check(&heap) == 0 && ack_calls == 0);

    reset(policy);
    CHECK(ael_page_acquire(&pool,&p) == 0); request(0,p,9);
    ack_error = RES_EBUSY;
    CHECK(ael_heap_check(&heap) == RES_EBUSY && p->state == AEL_PAGE_PENDING);
    CHECK(ack_calls == 1 && release_calls == 0);

    reset(policy);
    CHECK(ael_page_acquire(&pool,&p) == 0); request(0,p,10);
    events[0].handle.index = 99;
    CHECK(ael_heap_check(&heap) == RES_ESTALE && p->state == AEL_PAGE_MAPPED && ack_calls == 0);

    reset(policy);
    CHECK(ael_page_acquire(&pool,&p) == 0); request(0,p,11);
    info_error = RES_ENOENT;
    CHECK(ael_heap_check(&heap) == RES_ESTALE); /* INFO failure is not no-event. */

    reset(policy);
    CHECK(ael_page_acquire(&pool,&p) == 0); request(0,p,12);
    events[0].reason = 99;
    CHECK(ael_heap_check(&heap) == RES_EINVAL && ack_calls == 0);

    reset(policy);
    poll_error = RES_EINVAL;
    CHECK(ael_heap_check(&heap) == RES_EINVAL && heap.failure == RES_EINVAL);
    reset(policy);
    map_error = RES_ENOMEM; release_error = RES_EAGAIN;
    CHECK(ael_alloc(&heap,32) == NULL && heap.error == RES_EAGAIN && pool.owned == 1);
    map_error = release_error = 0;
    CHECK(ael_page_release(&pool,&pool.pages[0]) == 0);
}

static void sequences(enum ael_policy policy)
{
    reset(policy);
    uint32_t seed = 0x243f6a88;
    for (unsigned n = 0; n < 500; ++n) {
        seed = seed*1664525u+1013904223u;
        void *a = ael_alloc(&heap,1+seed%4096);
        CHECK(a && ((uintptr_t)a&15) == 0);
        *(volatile unsigned char *)a = (unsigned char)seed;
        CHECK(*(volatile unsigned char *)a == (unsigned char)seed);
        CHECK(ael_free(&heap,a) == 0);
        struct ael_page *p = heap.arenas[0].page;
        if (seed&1) { request(0,p,0x80001000u+n); CHECK(ael_heap_check(&heap) == 0); }
        else { CHECK(ael_heap_trim(&heap) == 0); }
        CHECK(pool.owned == 0 && heap.live == 0);
    }
    CHECK(ael_heap_destroy(&heap) == 0);
}

int main(void)
{
    abi();
    for (unsigned p = 0; p <= 1; ++p) { scenarios((enum ael_policy)p); sequences((enum ael_policy)p); }
    reset(AEL_FIRST_FIT);
    puts("PASS: revocation/profile scenarios and 1000 deterministic lifecycle sequences");
    printf("PASS: %u revocation assertions (ASan/UBSan)\n",checks);
    return 0;
}
