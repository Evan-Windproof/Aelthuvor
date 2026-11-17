/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef AELTHUVOR_PAGE_H
#define AELTHUVOR_PAGE_H
#include <aelthuvor/syscall.h>
#include <stddef.h>
#define AEL_PAGE_SLOTS ((RESOURCE_WINDOW_END - RESOURCE_WINDOW) / LIBERUMS_PAGE_SIZE)
/* Terminal states retain the old identity for diagnosis, never authority. */
enum ael_page_state { AEL_PAGE_FREE, AEL_PAGE_OWNED, AEL_PAGE_MAPPED,
                      AEL_PAGE_PENDING, AEL_PAGE_ACKED, AEL_PAGE_LOST, AEL_PAGE_RELEASED };
struct ael_page {
    struct resource_handle handle;
    uintptr_t address;
    unsigned owned, mapped;
    unsigned cleanup_pending; /* Failed acquire rollback; Trim must retry release. */
    enum ael_page_state state;
    uint32_t request;
};
struct ael_page_pool {
    struct ael_page pages[AEL_PAGE_SLOTS];
    unsigned quota, owned;
    unsigned revocation;
};
/* Only after confirmed reclaim or ESTALE; never dereferences page payload. */
void ael_page_retire(struct ael_page_pool *pool, struct ael_page *page, enum ael_page_state state);
int ael_pages_init(struct ael_page_pool *pool, unsigned quota);
int ael_page_claim(struct ael_page_pool *pool, struct ael_page **page);
int ael_page_map(struct ael_page_pool *pool, struct ael_page *page);
int ael_page_unmap(struct ael_page_pool *pool, struct ael_page *page);
int ael_page_release(struct ael_page_pool *pool, struct ael_page *page);
int ael_page_acquire(struct ael_page_pool *pool, struct ael_page **page);
#endif
