# Phase 5 boundary review

The production diff is limited to page rollback bookkeeping and Trim's retry
loop, plus an updated Finish comment. The allocator's placement and split/merge
rules, Runtime startup, legacy `ael_main`, SDK `ael_app_main`, cooperative and
preemptive profiles, device modules and independent reference retain their designs.

Liberums owns capability identity/generation/epoch, quotas, PTE authorization,
PIT deadlines, physical page zeroing, event cancellation, domain isolation and
final image/resource reclamation. Aelthuvor owns placement strategy, allocation
records, idle-vs-active ACK policy, checkpoints, rollback retries and application
error semantics. Application Context and Console are not kernel ABI components.
There are no Ael-specific syscalls, kernel grants or kernel source edits.

The integration target requires the exact peeled `v1.0.0` checkout and a clean
tracked kernel tree. It builds ignored kernel artifacts using existing targets,
then freezes kernel/worker/consumer ELF SHA256 **and mtime** through the audit.
Trusted `phase9_control` is enabled only by the existing `selftest=phase9` flag.
The suite also confirms that the diagnostic is inert without that flag and
rejects stale epochs, foreign handles and invalid deadlines. GDB writes only
that mailbox and private test-application scalar command inputs. No PTE,
capability, allocator metadata, counter or successful syscall frame is patched.

Commands 1..11 of the Ael-owned consumer derive from the GPL-licensed kernel
fixture, with attribution in `tests/fixtures/reliability.c`. Additional commands
exercise the SDK under pressure and actual CLAIM/MAP/UNMAP/RELEASE. The tests
reuse the pinned kernel's GDB audit helpers rather than weakening their MMU,
CPL3, TSS, IRQ EOI, physical free-count or terminal cleanup assertions.

The six vendored public headers were compared byte-for-byte with v1.0.0 and
with the per-file historical commits in `sdk/manifest.json`:

| Headers | Actual recorded source | v1.0.0 comparison |
| --- | --- | --- |
| elf.h, resource.h, syscall.h, event.h | `85a493466afe2038ca22a9870f6f9ce37dd7f3a8` | Identical |
| vga.h, keyboard.h | `5d9c8f35a4ae54ef08b93b7f4d5fecd9e350acbf` | Identical |

The manifest's six SHA256 digests pass `tests/check_sdk.py --source ...`; their
original provenance remains unchanged. ABI stays **Kernel 1.2 / Application 1.0**.
Historical ABI 1.1 ELF Notes are rebuilt from the original commit and tested as
such; no old Note is edited to obtain acceptance. v1.0.0's KBD_RELEASE error EDX
contract is also tested through a real CPL3 syscall probe.

Limits remain: no pin/lease API, no transparent migration, no safe resurrection
of old pointers, no SMP/threads, and no hour/day soak or physical PS/2 validation.
Finite seeded pressure is evidence of those sequences, not proof of unlimited
uptime. No Phase 6 work, GUI, network, filesystem, POSIX or IPC is included.
