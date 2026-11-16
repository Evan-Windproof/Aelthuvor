/* SPDX-License-Identifier: GPL-3.0-or-later */
#include <aelthuvor/revoke.h>

int ael_revoke_poll(struct ael_revoke_event *event)
{
    if (event == NULL) {
        return RES_EINVAL;
    }
    *event = (struct ael_revoke_event){0};
    uint32_t request = 0;
    int error = ael_call(SYS_REVOKE_POLL, 0, 0, 0, 0, &request);
    if (error != 0) {
        return error;
    }
    if (request == 0) {
        return RES_EINVAL;
    }
    struct ael_revoke_event found = {.request = request};
    uint32_t *fields[] = {&found.handle.index, &found.handle.generation,
                         &found.reason, &found.deadline};
    for (unsigned field = 0; field < 4; ++field) {
        error = ael_call(SYS_REVOKE_INFO, request, field, 0, 0, fields[field]);
        if (error != 0) {
            return error == RES_ENOENT ? RES_ESTALE : error; /* INFO is not an empty POLL. */
        }
    }
    if (found.handle.index == 0 || found.handle.generation == 0
        || (found.reason != REVOKE_REASON_PRESSURE && found.reason != REVOKE_REASON_TEST)) {
        return RES_EINVAL;
    }
    *event = found;
    return 0;
}

int ael_revoke_ack(const struct ael_revoke_event *event)
{
    if (event == NULL || event->request == 0) {
        return RES_EINVAL;
    }
    return ael_call(SYS_REVOKE_ACK, event->handle.index, event->handle.generation,
                    event->request, 0, NULL);
}

static struct ael_page *find_page(struct ael_page_pool *pool, struct resource_handle handle)
{
    for (unsigned i = 0; i < AEL_PAGE_SLOTS; ++i) {
        struct ael_page *page = &pool->pages[i];
        if (page->owned && page->handle.index == handle.index
            && page->handle.generation == handle.generation) {
            return page;
        }
    }
    return NULL;
}

static int arena_active(struct ael_heap *heap, struct ael_page *page)
{
    for (unsigned i = 0; i < AEL_PAGE_SLOTS; ++i) {
        struct ael_arena *arena = &heap->arenas[i];
        if (arena->page != page) {
            continue;
        }
        for (unsigned j = 0; j < arena->count; ++j) {
            if (arena->blocks[j].used) {
                return 1;
            }
        }
    }
    return 0;
}

static void forget_arena(struct ael_heap *heap, struct ael_page *page)
{
    for (unsigned i = 0; i < AEL_PAGE_SLOTS; ++i) {
        if (heap->arenas[i].page == page) {
            heap->arenas[i] = (struct ael_arena){0};
        }
    }
}

static int revoke_service(struct ael_heap *heap)
{
    struct ael_page_pool *pool = heap->pool;
    /* QUERY also detects missed force-completions, whose events disappear.
     * It cannot close the race between this checkpoint and a later user load. */
    for (unsigned i = 0; i < AEL_PAGE_SLOTS; ++i) {
        struct ael_page *page = &pool->pages[i];
        if (!page->owned) {
            continue;
        }
        int result = ael_call(SYS_PAGE_QUERY, page->handle.index,
                              page->handle.generation, 0, 0, NULL);
        if (result == RES_ESTALE) {
            int active = arena_active(heap, page);
            ael_page_retire(pool, page, AEL_PAGE_LOST);
            if (active) {
                return RES_ESTALE; /* Keep active metadata as quarantined evidence. */
            }
            forget_arena(heap, page);
        } else if (result < 0) {
            return result;
        }
    }

    uint32_t previous = 0;
    for (unsigned n = 0; n <= REVOKE_EVENTS_PER_DOMAIN; ++n) {
        struct ael_revoke_event event;
        int error = ael_revoke_poll(&event);
        if (error == RES_ENOENT) {
            return 0;
        }
        if (error != 0) {
            return error;
        }
        if (n == REVOKE_EVENTS_PER_DOMAIN || event.request == previous) {
            return RES_EBUSY; /* Non-consuming POLL must never make us spin. */
        }
        previous = event.request;
        struct ael_page *page = find_page(pool, event.handle);
        if (page == NULL) {
            return RES_ESTALE; /* Never mark a different generation/resource. */
        }
        page->state = AEL_PAGE_PENDING;
        page->request = event.request;
        if (arena_active(heap, page)) {
            return RES_EBUSY; /* No ACK, no migration: caller must stop the domain. */
        }
        error = ael_revoke_ack(&event);
        if (error != 0) {
            /* ACK may have raced force-completion. Identity is retained;
             * no retry/release/reuse is permitted in this uncertain state. */
            return error;
        }
        forget_arena(heap, page);
        ael_page_retire(pool, page, AEL_PAGE_ACKED);
    }
    return RES_EBUSY;
}

int ael_heap_check(struct ael_heap *heap)
{
    if (heap == NULL || heap->pool == NULL) {
        return RES_EINVAL;
    }
    if (heap->failure != 0) {
        return heap->failure;
    }
    if (heap->pool->revocation) {
        heap->failure = revoke_service(heap);
    }
    return heap->failure;
}
