# Phase 6 actual validation

Date: 2026-11-17. Development starts at Aelthuvor Phase 5
`ccdf850fba50df240b54f96f13d9fade8485f194`. The fixed independent kernel is
Liberums v1.0.0, peeled commit `7d5d574e8643c3645fa3ce2a126dcbd4ae43662f`.
Aelthuvor main matched the base, its worktree was clean, and there were no tags
or GitHub Releases at the preflight check. No kernel source or environment
configuration changes are required.

## Commands and tools

GCC 14.2.0, GNU Binutils 2.44, Make 4.4.1, Python 3.12.14, QEMU 10.0.13
(TCG, qemu32, one CPU, 32 MiB), GDB 16.3 with i386/Python. All commands run from
the Aelthuvor checkout; substitute the pinned kernel checkout and tool paths:

```sh
make clean
make -j2
make profiles
make test
make sdk-tests
make app-sdk
make apps
make independent-libos
make sdk-package
make package-tests
make integration LIBERUMS=/path/to/Liberums
make console-integration LIBERUMS=/path/to/Liberums
make phase4-integration LIBERUMS=/path/to/Liberums
make phase5-integration LIBERUMS=/path/to/Liberums
make phase6-integration LIBERUMS=/path/to/Liberums
python3 tests/check_sdk.py --source /path/to/Liberums
git diff --check
```

`make release-package` and `make release-tests` additionally require the final
clean committed source. release-tests extracts that source without .git,
rebuilds at another absolute path, compares **every release asset byte-for-byte**,
and requires a changed source file to be rejected for official packaging.
The final report records that post-commit result separately from ordinary SDK
package repeatability. Different toolchains are not claimed byte-identical.

## Passing regression coverage

| Layer | Actual evidence |
| --- | --- |
| Build | Default legacy ELF, all profiles/policies, static libraries, exported SDK, apps and independent reference |
| Memory host ASan/UBSan | 77,438,415 assertions |
| Revocation/reliability host ASan/UBSan | 5,532,619 assertions; 1,000 lifecycle sequences, 10,000 seeded pressure operations |
| Devices/Console host ASan/UBSan | 853 assertions |
| Application SDK host ASan/UBSan | 5,469 assertions, including 256 Context lifetimes |
| Independent pool host ASan/UBSan | 288 assertions |
| SDK provenance | Six per-file historical SHA256/source checks and v1.0.0 byte comparison; 10 drift rejection cases |
| Exported SDK builds | Existing isolated edited application, policy switching, unchanged core, entry collision rejection, isolated independent reference |
| Historical memory integration | 13 QEMU/GDB guests, 26 completed lifetimes; real historical ABI 1.1 and ABI 1.2 |
| Historical Console integration | 9 guests, 11 completed lifetimes + 1 persistent session; first/best, grants, queue/editing, commands, device revocation/cleanup |
| Phase 4 integration | 13 guests, 18 completed lifetimes; new/old entry, SDK Console, two LibOS policies and isolation |
| Phase 5 integration | All 27 guests retained, including actual idle ACK, PIT force, aliases removed, RAM zeroing, stale handles, exit 96, exact #PF, peer survival and 128 slot-reuse rounds |
| Phase 6 package/compiler audit | 16 package-only ELF builds, altered C changes ELF, no private dependencies, unchanged static SDK/core libraries |
| Phase 6 QEMU/GDB | 17 guests, 20 completed lifetimes; every tutorial executes |

These are unique case counts, not repeated-run totals. Host assertions are not
QEMU coverage. No completed lifetime is claimed for the persistent Console.
The kernel's unchanged native timer/revocation/ABI-contract checks also pass
when Phase 5 prepares its fixed diagnostic fixtures.

## Package and independent-application evidence

make package-tests inspects normalized tar metadata and safe member names,
SDK-FILES.json hashes, six-header provenance, symbol sets and lack of build-machine
paths. It actually injects an obsolete private object into the generated export,
then requires packaging to remove it and reproduce the same archive. Mutated
public headers and extra extracted files are rejected. Archives include no
private kernel objects, test logs, caches or prebuilt example ELF.

An isolated directory outside both repositories contains only the extracted SDK
and copied application template. The test builds the template, edits the C log
message, rebuilds four profile/policy combinations and verifies the ELF changed.
It checks compiler .d dependencies, link maps, unresolved symbols, ELF32/i386
headers and absence of optional device code from core-only programs. A repeat
build at a different OUT path with the same source produces identical bytes.
SDK library SHA256/mtime and original core library SHA256/mtime remain unchanged.
The actual changed application is executed; its unique message must appear in
serial output. This is not just a successful Make invocation.

The reference kernel is built once before the measured package-consumer interval.
Its ELF SHA256 and nanosecond mtime are frozen before outside compilation and
verified after every guest family completes. The final machine report includes
the exact SDK archive identity and the SHA256 of all sixteen executed ELF inputs.

## New guest cases

| Family | Guests / completed lifetimes | Machine-state checks |
| --- | --- | --- |
| Edited outside consumer, preempt first/best | 2 / 2 | Actual CPL3 entry, private image/stack/CR3, edited serial message, exit 0 and complete reclamation |
| Edited outside consumer, coop first/best | 2 / 4 | Existing external-repeat loader, two real lifetimes each, original device/space cleanup assertions |
| Nine noninteractive tutorial sources | 9 / 9 | Real entry/exit and cleanup; policy choices, OOM, Context, trim and project checks run inside the guests |
| Best-fit policy lesson | 1 / 1 | Same source selects smaller separated hole; no kernel change |
| Console lesson and interactive project | 2 / 2 | QMP -> PS2 -> real IRQ1 -> readline bytes 'hello'; actual VGA text, explicit device close and final cleanup |
| Lesson 8 plus independent LibOS | 1 / 2 | Separate loaded images/CR3s/stacks, neither domain depends on the other's runtime, both exit normally |

Lesson 8 is one of the nine noninteractive cases. At its observed stage 1 GDB
checks user CS/SS/CR3, real user-writable resource PTE, live/owned counts and all
1024 physical words `0xa6e10000 | i`. Stage 2 requires absent PTE, zero ownership,
cleared application pointer and zero/free physical RAM. Console observations
filter current_domain before checking user state; identical linear PCs in other
address spaces cannot masquerade as the observed application.

The pinned kernel audit helpers retain actual ELF/BSS/zero-stack permissions,
TSS/IOPL/IF, resource owner/epoch/generation, event/device cleanup, IRQ EOI and
physical free-count assertions. No PTE, capability owner, result frame or IRQ
counter is patched. New Console input is injected through QMP's real PS2 path.
The unchanged Phase 5 suite alone uses the existing gated diagnostic mailbox
for genuine RAM revocation; it is not an application runtime dependency.

## Evidence and release boundary

Generated reports live under build/: phase6-build-results.json,
phase6-results.json, release-repro-results.json, phase5-results.json,
phase4-results.json, phase4-build-results.json and phase3-results.json.
phase6-evidence/ and phase5-evidence/ contain local copies of GDB/serial logs
and command scripts. release-repro.log records the no-Git source rebuild.
Historical memory logs remain in the reference kernel's ignored build directory.
No generated logs, ELF, SDK archive or cache is committed to source.

Official release assets are source/SDK tarballs, release-manifest.json,
SHA256SUMS, RELEASE-NOTES.md and LICENSE. SOURCE.json in the source archive
records the exact committed file set and hashes; BUILD-INFO.json in the SDK
records source/toolchain/profile metadata. Development packages explicitly mark
a dirty source tree. The final tag must point to the tested clean commit; a tag
is not a GitHub Release. Preflight API requests to api.github.com were denied
by this session's network policy, while the public GitHub release page was
readable. Actual Release publication and asset accessibility must be reported
separately; this validation record does not assert they succeeded.

The public declarations and numeric versions remain Application API 1.0 /
Kernel ABI 1.2. Historical header provenance, ELF Notes and low-level retry
semantics are unchanged. No runtime, allocator, revoke or device implementation
rewrite was needed. Source review is summarized in sdk-reference.md.

Limits: no hour/day soak, physical keyboard or GTK-focus validation, no transparent
pointer recovery, and no proof covering every PIT interleaving. These results are
bounded, reproducible emulator and sanitizer evidence. Maintenance of the original
repository is handed to its maintainer; no Phase 7 is automatically started.
