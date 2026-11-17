# Known limitations

v1.0.0 is a tested educational/developer baseline, not production security or
unbounded uptime certification. It targets single-CPU i686, static freestanding
ELF32 on the pinned Liberums v1.0.0 kernel. No host libc, filesystem, network,
POSIX, fork/exec, dynamic loader, user spawn, generic IPC, SMP or threads.

Allocation calls accept 1..4096 bytes and align to 16 bytes; no realloc/calloc,
multi-page contiguous object or arbitrary alignment interface exists. Quotas
bound the page pool. Fragmentation can fail a request despite total free bytes.
First/best workload results are not a universal efficiency ranking.

Checks cannot pin RAM or devices. Forced revocation between a checkpoint and a
load/store can fault. Active pending/lost heap state exits 96; there is no
transparent migration or recovery of old pointers. Raw pointers cannot identify
old generations after virtual address reuse. C objects within one domain are
not isolated from each other. i686 lacks NX here; complete W^X is not claimed.

Console is exclusive text VGA + US translated PS/2 set 1, not a terminal/GUI or
portable keyboard stack. Trusted boot grants are required. Polling readline is
single-threaded; a queue overflow clears the current line. Buffer-full truncation
is separate. Device revocation can invalidate a later framebuffer access.

The multi launcher accepts at most four domains; cooperative modules use a
different launcher. The independent reference is another small LibOS, not an
alternative Ael SDK profile. Kernel-owned audit helpers are test dependencies,
not hidden application runtime interfaces.

Host sanitizers, bounded seeded tests, QEMU/GDB machine-state checks and archive
reproducibility are reported separately. No hour/day soak, GTK focus or physical
keyboard test was performed in this cloud. Make's SDK/project dependency paths
must not contain whitespace. Archive repeatability assumes the same source,
toolchain, build inputs and SOURCE_DATE_EPOCH; cross-toolchain binary equality
is not promised. Existing Phase 1–5 documents describe their historical stage.
