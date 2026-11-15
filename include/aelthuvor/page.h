/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef AELTHUVOR_PAGE_H
#define AELTHUVOR_PAGE_H
#include <aelthuvor/syscall.h>
#include <stddef.h>
#define AEL_PAGE_SLOTS ((RESOURCE_WINDOW_END - RESOURCE_WINDOW) / LIBERUMS_PAGE_SIZE)
struct ael_page {
    struct resource_handle handle;
    uintptr_t address;
    unsigned owned, mapped;
};
struct ael_page_pool {
    struct ael_page pages[AEL_PAGE_SLOTS];
    unsigned quota, owned;
};
int ael_pages_init(struct ael_page_pool *pool, unsigned quota);
int ael_page_claim(struct ael_page_pool *pool, struct ael_page **page);
int ael_page_map(struct ael_page_pool *pool, struct ael_page *page);
int ael_page_unmap(struct ael_page_pool *pool, struct ael_page *page);
int ael_page_release(struct ael_page_pool *pool, struct ael_page *page);
int ael_page_acquire(struct ael_page_pool *pool, struct ael_page **page);
#endif
