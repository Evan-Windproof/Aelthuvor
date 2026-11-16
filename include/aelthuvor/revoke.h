/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef AELTHUVOR_REVOKE_H
#define AELTHUVOR_REVOKE_H
#include <aelthuvor/memory.h>

/* All metadata is in image/stack memory, never in the revocable payload. */
struct ael_revoke_event {
    uint32_t request;
    struct resource_handle handle;
    uint32_t reason, deadline;
};
int ael_revoke_poll(struct ael_revoke_event *event);
int ael_revoke_ack(const struct ael_revoke_event *event);
/* Heap coordination is exposed only through sticky ael_heap_check(),
 * declared in memory.h. Raw protocol helpers do not update heap metadata. */
#endif
