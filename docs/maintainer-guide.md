# Maintainer handoff

Continue the original Evan-Windproof/Aelthuvor repository. No fork, new LibOS or
automatic Phase 7 is part of this delivery. Release v1.0.0 fixes Application API
1.0 and its documented limits as a teaching/developer baseline. Keep Liberums
independent; kernel changes require work in its own project and review process.

## Daily work and pull requests

```sh
git status
git remote -v
git fetch origin --tags
git switch main
git pull --ff-only origin main
git switch -c fix/descriptive-name
make -j2 profiles test
make sdk-tests package-tests
git diff --check
```

Preserve local changes before switching. `make clean` removes generated build/,
including SDK exports, packages, JSON evidence and screenshots; save evidence
needed for a review first. It does not remove the reference kernel build.
For a PR, explain the trigger, corrected behavior, compatibility impact and
tests. Keep allocator/device policy out of the kernel. Review actual diffs,
not assertion totals; never remove a safety assertion just to make CI pass.
Run all integration targets before merging release-affecting changes. Use a
branch/PR if remote main has moved; never force-push to resolve divergence.

## Public SDK changes

Review app.h/app_console.h against [SDK reference](sdk-reference.md). Preserve
existing signatures, error values and opaque Context. An additive API needs
documentation, source-compatibility tests and a conscious API version decision;
a kernel version bump is not a reason to bump the Application API. Internal
page/heap layouts are advanced interfaces, not the stable ordinary app contract.

Never put an application main, startup or adapter into libaelthuvor.a or
libaeldevices.a. Keep crt0.o, app-entry.o and the four runtime objects explicit.
Update export/package inputs when adding a header or object. Export removes
obsolete generated files; do not place hand-maintained files in build/app-sdk.
Run make app-sdk, sdk-tests, package-tests and phase6-integration after changes.

sdk/manifest.json is the per-file kernel-header authority. `tests/check_sdk.py`
rejects missing/extra headers, hash drift and bad ABI constants. With
`--source /path/to/Liberums`, it compares historical Git blobs and current
public headers. Do not import private kernel headers or replace historical
source commits mechanically. A deliberate public ABI import must record each
actual source commit, SHA256 and license, and retain the old provenance in Git.

## Memory and failure checklist

Page Pool counts owned `(index,generation)` capabilities. Heap arenas partition
mapped pages; blocks and live counts remain evidence even when an active page
is lost. First-fit and Best-fit must pass identical partition/data/accounting
invariants under fragmentation and quota pressure. Never turn an OOM into a
non-NULL invalid pointer or erase ownership on a failed release.

MAP+rollback failure retains exactly one owner with cleanup_pending. Trim must
retry it; explicit successful low-level MAP clears the marker and transfers
responsibility back to that low-level caller. Generation equality must include
all 32 bits. Idle revoke may ACK; active revoke/loss must retain records and
Fail-stop. POLL is non-consuming and must be bounded. Check is not a memory pin.
Device capabilities are never RAM pages. Kernel EXIT/fault is the final cleanup
authority, not a replacement for reporting application cleanup errors.

## Support triage

| Symptom | First checks |
| --- | --- |
| Linker cannot find entry | Implement ael_app_main for SDK adapter; do not add host main or both adapters |
| Undefined printf/malloc | Remove host-libc assumptions; use SDK log/alloc and explicit error handling |
| ABI rejection / exit 93 | Read ELF Note, profile/launcher and actual kernel features; never edit an old Note to force acceptance |
| NULL allocation | Read last-error immediately; inspect quota, fragmentation and request size |
| Exit 95 | Look for live allocations, remaining raw owners or failed device/page cleanup |
| Exit 96 / #PF | Inspect pending/lost resources, full handle/request, PIT timing and exact fault EIP/CR2 |
| Console EPERM | Check trusted multi-vga / multi-keyboard grants and APP_DEVICES, not special kernel privileges |
| Readline overflow | Distinguish queue loss (-75, cleared line) from a full caller buffer |
| SDK package mismatch | Verify SHA256SUMS, SDK-FILES and six-header provenance before rebuilding |

Request a bounded reproducer, toolchain versions, source SHA, profile/policy,
serial and GDB logs. Follow [debugging](debugging-guide.md); verify actual CPL3,
CR3, resource accounting and peer-domain completion. Physical GTK/keyboard and
long-duration soak claims need their own evidence and cannot be inferred from
QMP or sanitizer success.

## Publish a patch release

1. Check existing remote tags and Releases. Choose a new unoccupied semantic
   VERSION; never overwrite v1.0.0 or an earlier asset set.
2. Update CHANGELOG, RELEASE-NOTES and compatibility/validation records. Run the
   full clean regression and external SDK consumers against the pinned kernel.
3. Review licenses, exported file set and git diff --check. Commit and push by
   fast-forward; verify the remote SHA. Official packaging requires a clean tree.
4. Run `make release-package` twice at that final commit and compare hashes.
   `make release-tests` extracts the source without .git, rebuilds at another
   path and checks every release asset byte-for-byte, then rejects modified source.
   Run package-tests and phase6-integration against that SDK. If testing invokes
   sdk-package, regenerate release-package afterward to restore the source asset.
5. Verify SHA256SUMS in build/release and the source/SDK commit in the manifest.
   Inspect tar members for build/cache/kernel artifacts. Archive hashes cover
   all supplied assets; GitHub-generated automatic source downloads may differ.
6. Create an annotated tag pointing to the final tested commit, push it without
   force and verify its peeled remote commit. Example for a future version:
   `git tag -a v1.0.1 -m 'Aelthuvor v1.0.1'` then `git push origin v1.0.1`.
7. With GitHub Release API permission, use `gh release create v1.0.1 --verify-tag
   --title 'Aelthuvor v1.0.1' --notes-file RELEASE-NOTES.md build/release/*`.
   Check public release and asset downloads; compare downloaded SHA256SUMS.

A pushed tag is not a GitHub Release. If API/network permission blocks release
publication, retain the commit, tag, built assets and notes; report the exact
remaining step. A maintainer can upload those same verified assets later. Do not
publish partially tested binaries or silently substitute a later commit's SDK.

Future bug fixes, API maintenance, allocator improvements, Console refinements,
external applications and documentation are welcome. Major architecture changes
belong to the maintainer; the project does not automatically become POSIX.
