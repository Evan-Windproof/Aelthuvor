# Phase 5 revocation

`src/revoke.c` checks every owned page with PAGE_QUERY before servicing events.
ESTALE means the kernel has already retired that generation. The pool decrements
owned and marks LOST. Empty arenas can be detached; active arenas keep their
block records and `heap.live`, and the heap caches ESTALE. Metadata resides in
the ELF data region, outside the resource mapping being revoked.

POLL is non-consuming. The client collects INFO index, generation, reason and
deadline into a temporary structure; a partial snapshot is never published.
INFO ENOENT becomes ESTALE, not a successful empty queue. Index and generation
must match an owned record exactly. An unrelated event cannot cause ACK of a
different resource. A pass services at most the ABI's two events, with a final
bounded POLL; a repeated request returns EBUSY instead of looping forever.

An idle arena has no used blocks. Only then can the service send ACK with the
full `(index, generation, request)` identity. It forgets the arena and retires
the page only after success. ACK ESTALE can mean deadline completion raced the
call; PENDING identity and a sticky failure prevent retries, RELEASE or reuse.
The kernel enforces deadlines using PIT ticks; the LibOS does not guess current
time from the INFO deadline.

An active arena has at least one used block, even if another block was freed.
It cannot be safely ACKed: an application may retain arbitrary interior pointers.
The client records PENDING and returns sticky EBUSY without erasing allocations.
Public allocator and SDK gates exit 96 on an unsafe heap:

```c
if (arena_active(heap, page)) {
    return RES_EBUSY; /* No ACK, no migration: caller must stop the domain. */
}
```

`ael_heap_check()` itself returns the diagnostic error. `ael_app_check()` cannot
return it to an application that may use dangerous pointers: it exits 96.
After force completion the request disappears, so QUERY is essential. Checks
cannot pin memory: a PIT deadline may expire immediately after a successful
checkpoint and before an application load. The expected fallback is kernel
isolation and #PF, not transparent pointer recovery.

The real-kernel suite verifies full POLL/INFO/ACK syscall frames, both aliases
removed, dynamic page table released, 1024 zero words, free resource state,
event retirement, and stale handle rejection. Wrong generation/request ACK,
duplicate ACK, late ACK, pending RELEASE, EXIT and fault cleanup are distinct
cases. Fresh SDK allocation after idle ACK or idle forced loss proves that an
expired arena is detached before new ownership is installed. Active loss exits
96; a diagnostic old-pointer read instead produces vector 14, error 4 at the
exact `ram_fault_read` instruction. Independent LibOS and worker survivors must
all exit normally.
