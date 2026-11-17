# Phase 5 failure handling

Application API 1.0 still uses NULL for allocation failure and named negative
errors for recoverable failures. `ael_app_error()` reports the last operation;
a later successful checked operation clears it. `get_info` is a snapshot, not
a lifetime guarantee. Kernel ABI version and Application API version are separate.

| Trigger | Observable behavior | Retained state / cleanup |
| --- | --- | --- |
| Zero/oversized allocation | NULL, EINVAL | No new resource |
| Quota/OOM | NULL, ENOSPC/ENOMEM | Existing allocations remain usable |
| MAP failure, rollback succeeds | NULL, original MAP error | Capability retired, no arena |
| MAP and rollback fail | NULL, actual RELEASE error | One owner, cleanup marker, no arena |
| Trim retry fails | Actual RELEASE error | Handle and marker retained for retry |
| Trim retry succeeds | 0 | Marker cleared, owned decremented exactly once |
| Explicit low-level MAP retry succeeds | 0 | Marker cleared; standalone owner requires explicit release |
| Invalid/interior/double free | EINVAL | No block/count mutation |
| Live objects at Finish | EBUSY, context closed | Allocations retained until kernel EXIT |
| Cleanup failure at Finish | Actual cleanup error, context closed | Remaining ownership visible to kernel teardown |
| Closed context operation | EINVAL / NULL | No syscall or allocation |
| Active pending/lost heap | exit 96 | Keep active metadata; no unsafe pointer returned |
| Successful app return with cleanup error | exit 95 | Distinct from unsafe heap exit 96 |

Finish closes optional devices and attempts heap destruction even when device
close fails. It prefers the device error if both fail. A caller-supplied nonzero
exit status keeps precedence over ordinary Finish errors; unsafe heap detection
always terminates with 96. Finish closes the context even on a recoverable
cleanup error: retry cleanup with Trim **before** Finish, or let kernel EXIT
reclaim the domain. Neither Finish nor free is a promise to preserve arbitrary
stale raw pointers after address reuse.

The rollback path retains ownership until confirmed release:

```c
int cleanup = ael_page_release(pool, *out);
if (cleanup != 0) {
    (*out)->cleanup_pending = 1;
    return cleanup;
}
```

This internal marker does not change `app.h`, public symbols or Application API
1.0. `ael_page_map` clears it only on explicit successful recovery, preserving
the older low-level retry contract. Trim ignores unrelated standalone pages;
Finish's final `pages.owned` check prevents false success over such owners.

Host failure injection covers persistent and transient rollback failures,
ordinary release failure, active loss, unsafe API gates, and device acquire /
partial-init / stale-service cleanup. Device pages never enter the ordinary RAM
Page Pool. Device faults stop their service; unsafe RAM faults stop the domain.
