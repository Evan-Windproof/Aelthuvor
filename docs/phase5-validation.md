# Phase 5 actual validation

Executed 2026-11-17, against Aelthuvor Phase 4 base `297d7a3` and the exact
Liberums `v1.0.0` commit `7d5d574e8643c3645fa3ce2a126dcbd4ae43662f`.
GCC 14.2.0, Binutils 2.44, Make 4.4.1, Python 3.12.14, QEMU 10.0.13
TCG/qemu32/one CPU/32 MiB and Python-enabled i386 GDB 16.3 were used. The cloud
environment configuration was not changed or published.

## Reproduce

The reference checkout must already be at the exact tag, with no tracked source
changes. Ordinary installations may use `qemu-system-i386` and `gdb` from PATH.
This cloud used `/tmp/aelthuvor-liberums-reference` and
`/workspace/aelthuvor-tools/bin/{qemu-system-i386,gdb}`.

```sh
make clean
make -j2
make profiles
make test
make sdk-tests
make phase4-integration LIBERUMS=../Liberums
make integration LIBERUMS=../Liberums
make console-integration LIBERUMS=../Liberums
make phase5-integration LIBERUMS=../Liberums
python3 tests/check_sdk.py --source ../Liberums
git diff --check
```

The first native regression after adding a restrictive MAP guard caught an
existing supported recovery: explicitly remapping a failed acquire. The guard
was removed, the success path clears the cleanup marker, and the original test
was retained. The final implementation passes that regression and a new
assertion that Trim does not reclaim the recovered standalone owner.

The original Trim defect was reproduced **before** the fix: the new assertion
`ael_heap_trim(&heap) == RES_EAGAIN && pool.owned == 1` failed because Trim
reported success without trying RELEASE. The final tests require the real
cleanup error, then successful retry with exactly one owner retired.

## Host and historical results

| Target / layer | Actual passing result |
| --- | --- |
| clean, default build, profiles | Legacy entry; cooperative/preemptive; both policies |
| Memory ASan/UBSan | 77,438,415 assertions |
| Revocation/reliability ASan/UBSan | 5,532,619 assertions; original 1,000 lifecycle sequences plus 10,000 new pressure operations |
| Devices/Console ASan/UBSan | 853 assertions |
| Application SDK ASan/UBSan | 5,469 assertions; includes 256 seeded Context lifetimes and unsafe/closed API gates |
| Independent pool ASan/UBSan | 288 assertions |
| SDK checks | Six historical per-file SHA256/source comparisons, v1.0.0 byte comparison; 10 drift rejection tests |
| sdk-tests | Export-only external consumer build/edit/rebuild, unchanged core, policy switching, duplicate-entry rejection, isolated independent LibOS |
| Historical memory integration | 13 guests, 26 completed lifetimes, original ABI 1.1/1.2 checks retained |
| Phase 4 integration | 13 guests, 18 completed lifetimes, opaque SDK apps and independent LibOS coexistence |
| Console integration | 9 guests, 11 completed lifetimes + one persistent session; first/best, PS/2, VGA, editing, commands, grant denial, device revocation and cleanup |
| Phase 5 integration | 27 real QEMU/GDB guests, including 128 slot-reuse rounds and 512 guest pressure operations per policy |
| Pinned kernel native contracts | 1,008 timer, 1,321 revocation assertions; frozen 26-syscall ABI contract |

The host pressure seed is `0x5a17c0de`: each policy runs 5,000 operations with
32 potential objects and quota four. First-fit encounters 2,100 expected OOMs
and 12 idle ACKs; Best-fit encounters 2,104 OOMs and 10 idle ACKs. These are
properties of this workload, not performance rankings. Each step independently
checks actual mock owners, unique resource indices/generations, page counts,
arena uniqueness, contiguous aligned partitions, coalescing, live counts and
payload patterns. Transient/persistent MAP/rollback failures are interleaved
with cleanup. Separate cases force active/idle losses and uncertain ACKs.
The original allocator assertions are not deleted or relaxed.
The real guest workload uses 24 potential objects and retains its original live
allocation: First-fit encounters 151 expected OOMs and Best-fit 167, across
512 operations each. Both restore ownership to the single original page.

Context stress uses the same seed, 128 lifetimes per policy. It exercises
fragmentation-sized allocations, failed initialization rollback, cleanup retries,
snapshots, invalid/double free, and every closed-context gate without syscalls.
Unsafe heap gates for check/free/trim/get_info/finish/exit must terminate with 96.

## Real kernel evidence

The 27 guests comprise five pinned kernel/core audits, sixteen Ael RAM audits
(eight per policy), and six new pressure/lifecycle/reuse audits (three per
policy). All Ael RAM cases execute our independently linked extended public-SDK
consumer; core-only worker audits are identified separately.

| Case family | Required state evidence |
| --- | --- |
| ABI / diagnostic core | Actual KBD_RELEASE error EAX/EDX; stale epoch/foreign identity denial; diagnostic disabled without flag |
| Slot reuse core | 128 EXIT/#UD cycles, incremented epochs, old-generation rejection, zero/free pages and stable counts |
| Idle ACK | Real CPL3 POLL + four INFO fields + matching ACK; acknowledged=1, forced=0 before deadline |
| Active pending | No ACK; actual exit status 96; all other domains exit 0 |
| Active forced loss | Real PIT deadline, both aliases/PDE removed, physical RAM zero/free; SDK exits 96 |
| Forced old pointer read | Expected #PF vector 14 / error 4 at exact consumer instruction; peers survive |
| Late/duplicate ACK | ESTALE after completion, no resurrected event/owner |
| Pending RELEASE / EXIT / fault | Request cancelled by real cleanup; no leftover event or binding |
| Normal + pressure | 512 seeded operations/policy reach quota; 1024 actual CPL3 word writes; UNMAP retains ownership/data; REMAP preserves data; RELEASE zeros/frees and QUERY rejects old handle |
| ACK reuse | Wrong-generation/request ACK rejected; valid idle ACK, then fresh SDK allocation/trim with zero stale ownership |
| Idle force reuse | Real deadline reclamation, SDK detaches lost empty arena, new zero-filled allocation and clean trim |

For example, the first-fit ACK GDB audit recorded:

```text
REQUEST id=1 epoch=1 handle=(1, 1) request=1 now=3 deadline=1003
RECLAIM handle=(1, 1) PA=0x1a6000 PTE=0 state=PAGE_FREE zero_words=1024 acknowledged=1 forced=0 cancelled=0
EXTERNAL LIBOS protocol trace= [(24, 0, 0, 0), (25, 1, 0, 0), (25, 1, 1, 0), (25, 1, 2, 0), (25, 1, 3, 0), (26, 1, 1, 1)]
```

Physical addresses and PIT tick values describe this execution, not ABI promises.
The final `finished()` assertions require all slots unused, all sixteen resource
slots free, all eight events inactive, device/queue state empty, IRQ EOI accounting
correct, and actual physical free count restored to the pre-launch value.
The independent LibOS retains its own runtime and object-pool policy and must
exit normally even when Aelthuvor exits 96 or faults.

## Artifacts and debugging

`build/phase5-results.json` records each case, input SHA256 and nanosecond mtime,
kernel pin and local evidence paths. GDB/serial logs and replayable `.gdb`
commands are copied to `build/phase5-evidence/`. Historical reports remain
`build/phase4-results.json`, `phase4-build-results.json` and `phase3-results.json`.
Historical memory integration writes GDB/serial logs in the reference kernel's
ignored build directory rather than an Ael JSON report. Generated artifacts
are ignored, not committed.
The `.gdb` files refer to the dynamically assigned QMP endpoint; reproduction
should launch the target rather than reuse an expired endpoint.

The audited v1.0.0 kernel ELF SHA256 is
`6e3d2066bc197cb76bb3bda555d4a9edf3b30083d1fe71b557ab52aaba841f86`.
Different checkout paths/toolchains may change ELF debug bytes. Kernel and
consumer input SHA256/mtime are frozen throughout Phase 5 execution. The source
tree stays clean; no Liberums commit or push is performed.

To inspect the ready-made commands, locate a log's `.gdb` companion and its
hardware stop at `revoke_completed`. The actual audit reads:

```python
check(pt(1, 0x09000000) == 0 and pt(1, 0x09001000) == 0, 'aliases survived')
check(v('states[%d]' % (pa >> 12)) == 2 and not any(words(pa, 1024)), 'not free/zero')
```

This is finite, reproducible pressure and actual hardware-state observation in
an emulator. It is not an hour/day soak, physical-device test or exhaustive
interleaving proof. WSL2 GTK/physical keyboard and unbounded uptime remain
outside this validation. No failing safety assertions were removed or skipped.
