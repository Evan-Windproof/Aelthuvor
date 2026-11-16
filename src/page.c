/* SPDX-License-Identifier: GPL-3.0-or-later */
#include <aelthuvor/page.h>

static int valid_page(struct ael_page_pool *pool, struct ael_page *page)
{
    if (pool != NULL) {
        for (unsigned i = 0; i < AEL_PAGE_SLOTS; ++i) {
            if (page == &pool->pages[i]) {
                return page->owned && (page->state == AEL_PAGE_OWNED || page->state == AEL_PAGE_MAPPED);
            }
        }
    }
    return 0;
}

int ael_pages_init(struct ael_page_pool *pool, unsigned quota)
{
    if (pool == NULL || quota == 0 || quota > AEL_PAGE_SLOTS) {
        return RES_EINVAL;
    }
    /* Initialization is only valid for a fresh pool, never a live allocator. */
    *pool = (struct ael_page_pool){.quota = quota};
    return 0;
}

int ael_page_claim(struct ael_page_pool *pool, struct ael_page **out)
{
    if (pool == NULL || out == NULL || pool->quota == 0) {
        return RES_EINVAL;
    }
    *out = NULL;
    if (pool->owned >= pool->quota) {
        return RES_ENOSPC;
    }
    for (unsigned i = 0; i < AEL_PAGE_SLOTS; ++i) {
        struct ael_page *page = &pool->pages[i];
        if (page->owned) {
            continue;
        }
        uint32_t generation = 0;
        uint32_t rights = RESOURCE_READ | RESOURCE_WRITE | RESOURCE_MAP | RESOURCE_RELEASE | RESOURCE_QUERY;
        int32_t index = ael_call(SYS_PAGE_CLAIM, rights, 0, 0, 0, &generation);
        if (index < 0) {
            return index;
        }
        *page = (struct ael_page){.handle = {(uint32_t)index, generation}, .owned = 1, .state = AEL_PAGE_OWNED};
        ++pool->owned;
        *out = page;
        return 0;
    }
    return RES_ENOSPC;
}

int ael_page_map(struct ael_page_pool *pool, struct ael_page *page)
{
    if (!valid_page(pool, page)) {
        return RES_ESTALE;
    }
    if (page->mapped) {
        return RES_EBUSY;
    }
    /* Address choice belongs to the LibOS. A busy VA may have been used by
     * another user component, so let the kernel arbitrate and try the next. */
    for (uintptr_t address = RESOURCE_WINDOW; address < RESOURCE_WINDOW_END; address += LIBERUMS_PAGE_SIZE) {
        int error = ael_call(SYS_PAGE_MAP, page->handle.index, page->handle.generation,
                             (uint32_t)address, RESOURCE_READ | RESOURCE_WRITE, NULL);
        if (error == RES_EBUSY) {
            continue;
        }
        if (error != 0) {
            return error;
        }
        page->address = address;
        page->mapped = 1;
        page->state = AEL_PAGE_MAPPED;
        return 0;
    }
    return RES_ENOSPC;
}

int ael_page_unmap(struct ael_page_pool *pool, struct ael_page *page)
{
    if (!valid_page(pool, page)) {
        return RES_ESTALE;
    }
    if (!page->mapped) {
        return RES_ENOENT;
    }
    int error = ael_call(SYS_PAGE_UNMAP, (uint32_t)page->address, 0, 0, 0, NULL);
    if (error == 0) {
        page->state = AEL_PAGE_OWNED;
        page->mapped = 0;
        page->address = 0;
    }
    return error;
}

int ael_page_release(struct ael_page_pool *pool, struct ael_page *page)
{
    if (!valid_page(pool, page)) {
        return RES_ESTALE;
    }
    /* RELEASE revokes all bindings. Forget the handle only after success;
     * an error must leave enough metadata to retry or let domain teardown act. */
    int error = ael_call(SYS_PAGE_RELEASE, page->handle.index, page->handle.generation, 0, 0, NULL);
    if (error == 0) {
        ael_page_retire(pool, page, AEL_PAGE_RELEASED);
    }
    return error;
}

int ael_page_acquire(struct ael_page_pool *pool, struct ael_page **out)
{
    int error = ael_page_claim(pool, out);
    if (error == 0) {
        error = ael_page_map(pool, *out);
        if (error != 0) {
            int cleanup = ael_page_release(pool, *out);
            if (cleanup != 0) {
                return cleanup; /* The live record remains in the pool. */
            }
            *out = NULL;
        }
    }
    return error;
}

void ael_page_retire(struct ael_page_pool *pool, struct ael_page *page, enum ael_page_state state)
{
    if (page->owned) {
        --pool->owned;
    }
    page->owned = page->mapped = 0;
    page->address = 0;
    page->state = state;
}
