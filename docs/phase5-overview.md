# Phase 5 — Resource Reliability & Revocation Lifecycle

Phase 5 builds on Phase 4 `297d7a3e2852b495b66f4a2b585b461e1bcc28b7`.
The reference kernel is the peeled Liberums `v1.0.0` commit
`7d5d574e8643c3645fa3ce2a126dcbd4ae43662f`. Its annotated tag object is
`444dbe54ef59c61f828cfc425b97d5b15721216d`. Both remote mains matched these
baselines at the development review. Only Aelthuvor source changes are required.

The lifecycle review identified a narrow recovery gap: failed MAP plus failed
rollback RELEASE retained the capability but created no arena, so Trim could
return success without retrying it. `ael_page.cleanup_pending` now identifies
these records. Trim retries their RELEASE before visiting empty arenas. Failed
retries preserve the handle and return the actual error; successful retirement
clears the marker. The existing explicit low-level MAP retry remains supported.
The public opaque Application SDK requires no interface or version change.

First-fit and Best-fit algorithms, alignment, splitting/coalescing, startup and
both entry adapters remain intact. Active revocation still requires Fail-stop.
The existing revocation client's identity matching, complete INFO snapshot,
bounded POLL and sticky failure handling pass host and real-kernel audits; they
did not require a rewrite. Device implementations and the independent LibOS are
unchanged. No resource policy moves into the kernel.

New evidence includes 10,000 seeded host pressure operations, 256 Application
Context lifetimes, and 27 QEMU/GDB guests against the fixed kernel. The guest
consumer uses only exported `app.h` and public ABI syscalls. It also runs 512
seeded operations per allocator. Kernel-owned helpers from the exact tag audit
the trusted diagnostic, page tables, physical RAM, CPL3 and lifecycle counters.
No debugger writes fabricate resources, mappings, ACK success or timer IRQs.

See [lifecycle](phase5-resource-lifecycle.md), [revocation](phase5-revocation.md),
[error semantics](phase5-failure-handling.md), [validation](phase5-validation.md)
and [boundaries](phase5-boundary-review.md). Stop at Phase 5: this is neither a
Phase 6 release nor an API stabilization or long-duration soak claim.
