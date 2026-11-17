# Phase 5 resource lifecycle

The review baseline is Aelthuvor `297d7a3` and the peeled Liberums `v1.0.0`
commit `7d5d574e8643c3645fa3ce2a126dcbd4ae43662f`. Page/arena metadata is
outside resource RAM; losing a mapping does not itself destroy allocation records.

| Operation | Page Pool | Heap / application | Failure boundary |
| --- | --- | --- | --- |
| CLAIM | Store full index/generation, increment owned | No arena yet | Failed claim creates no owner |
| MAP | Record address only after success | Create arena only after acquire succeeds | Failed MAP tries RELEASE; failed rollback retains the owner |
| Allocate | Mapped page remains owned | Split a free block; increment live | OOM returns NULL, never a partially acquired pointer |
| Free | Resource remains mapped and owned | Merge blocks; decrement live | Invalid/interior/double free returns EINVAL |
| Trim | RELEASE empty arenas and failed-acquire records | Forget arena only after release succeeds | Preserve failed cleanup for retry |
| Idle revoke | PENDING identity, ACK, then ACKED and owned-- | Remove empty arena after ACK succeeds | Failed ACK quarantines heap; never retry automatically |
| Active revoke | PENDING, no ACK | Preserve live blocks and pointers as evidence | Sticky EBUSY; public entry exits 96 |
| Forced revoke | QUERY ESTALE, LOST, owned-- | Empty arena detached; active arena retained | Active loss caches ESTALE; public entry exits 96 |
| Finish | Try device close and heap destruction | Live allocations produce EBUSY; close context | Cleanup errors propagate; zero app status becomes exit 95 |
| EXIT / fault | Kernel cancels events and removes every binding | No further execution of this context | Kernel zeros/releases remaining resource and image RAM |

Page Pool accounts for capabilities, whereas the heap accounts for allocations.
`owned == 0` does not imply `live == 0`: an active lost page deliberately keeps
its allocation evidence until terminal kernel cleanup. Ordinary free relinquishes
an allocation, not a capability. ACK relinquishes the entire resource and all aliases.

The initial review found that failed MAP + failed RELEASE was tracked correctly
but unreachable by Trim: no arena had been constructed. Phase 5 marks precisely
these failed-acquire records for cleanup, rather than sweeping unrelated pages
claimed through the low-level page API. Trim retries them; a second failure keeps
the record and returns the actual RELEASE error. Successful retirement clears the
marker. The existing low-level API can also explicitly retry MAP: success clears
the cleanup marker and makes the page a standalone owner. Trim does not reclaim
that owner's page. Heap allocation never silently adopts a failed acquire.
