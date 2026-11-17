# Architecture

**Kernel protects resources; LibOS manages resources.** Aelthuvor is a static
Ring 3 library OS and SDK, optional to Liberums. It neither boots other domains
nor becomes a dependency of every program on the kernel.

```text
Application: ael_app_main + opaque app.h / optional app_console.h
       | core SDK                           | optional devices
Runtime / Context -> Heap -> Page Pool       VGA / keyboard / text
       |                    |                    |
       +-------- public scalar Kernel ABI 1.2 ----+
                            |
Liberums: capability/epoch/generation, MMU, PIT, grants, revocation, teardown
```

crt0 aligns the loader's private stack and calls ael_start. Runtime checks ABI
and feature compatibility, discovers resource limits, initializes one Page Pool
and Heap, then invokes ael_main. For SDK applications an explicitly linked
app-entry.o implements that legacy adapter and calls ael_app_main; archives
contain neither entry. Profiles and policies select one of four runtime objects.

Each arena partitions one 4096-byte resource page into aligned blocks. First-fit
selects the first suitable hole; Best-fit scans for the smallest. Both reuse
space before claiming another capability. Metadata resides in ELF data, not
revocable payload. Free coalesces blocks; Trim releases empty arenas and retries
marked failed-acquire records. Failed release preserves identity and counters.

Checkpoints query ownership and service Visible Revocation. Idle resources may
be ACKed; active ones cause sticky Fail-stop. The kernel removes all aliases and
zeros actual physical RAM on completion. No user policy can override protection.
Optional device capabilities are separate from ordinary RAM and never enter the
page allocator. The independent reference implements its own fixed-object pool.

Application declarations are documented in [SDK reference](sdk-reference.md).
Historical implementation detail remains in [Phase 1–5 index](history.md),
especially the Phase 5 lifecycle and revocation review. No runtime or allocation
algorithm rewrite was needed for the v1.0.0 SDK release.
