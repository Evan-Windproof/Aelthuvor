# Aelthuvor — independent Library OS & Application SDK

[简体中文](README.zh-CN.md) · [繁體中文](README.zh-TW.md)

Aelthuvor is a C11/i686 Ring 3 Library OS for Liberums. **Kernel protects resources;
LibOS manages resources.** It is optional to the kernel; other LibOS implementations
can run without its headers or libraries. GPL-3.0-or-later.

The v1.0.0 teaching/developer baseline retains **Application API 1.0** and targets
**Liberums v1.0.0 / Kernel ABI 1.2**, commit
`7d5d574e8643c3645fa3ce2a126dcbd4ae43662f`. These version systems are independent.
Phase 6 completes SDK packaging, developer lessons and maintainer handoff. A source
tag and a published GitHub Release are separate states; inspect the actual
[release page](https://github.com/Evan-Windproof/Aelthuvor/releases) for hosted assets.

## Build and develop

Linux, GCC with -m32/i686 code generation, GNU Binutils/Make and Python 3.12+ are
required. No 32-bit host libc is needed for target binaries. Host tests use native
ASan/UBSan; integration needs QEMU system i386 and i386/Python-enabled GDB.

```sh
make -j2
make profiles app-sdk apps independent-libos
make sdk-package                 # build/release/aelthuvor-sdk-1.0.0.tar.gz
```

The SDK contains public headers, two static libraries, four runtimes, explicit
startup/entry objects, linker/build rules, provenance, license, template, examples
and documentation. It contains no kernel, VM or unrelated application ELF.
Verify SHA256SUMS before extracting a downloaded package, then create your own
project outside either repository:

```sh
export SDK=/absolute/path/to/aelthuvor-sdk-1.0.0
cp -R "$SDK/templates/application" my-app
make -C my-app AEL_SDK="$SDK"
make -C my-app AEL_SDK="$SDK" APP_POLICY=best
```

Implement ael_app_main with the borrowed opaque Context. App development does not
read private LibOS/kernel source or relink the kernel. Select APP_PROFILE=preempt|coop,
APP_POLICY=first|best and APP_DEVICES=0|1. Defaults are preempt/first/no devices.
See the [ten-lesson course](docs/application-development/README.md),
[API reference](docs/sdk-reference.md) and [build guide](docs/build-guide.md).
Use paths without whitespace in the installed Make rules.

## Run and inspect

Build the pinned Liberums kernel once in its own repository. For preemptive ELF:

```sh
qemu-system-i386 -machine pc -accel tcg -cpu qemu32 -m 32M -smp 1 \
  -display none -serial stdio -monitor none -nic none -no-reboot -no-shutdown \
  -kernel /path/to/Liberums/build/liberums.elf -append multi-external \
  -initrd my-app/build/app.elf
```

Cooperative ELF uses -append external. Console additionally needs trusted
multi-vga=1 multi-keyboard=1 grants and APP_DEVICES=1. The SDK Console and historical
Console retain help/clear/about/status/memtest/exit. QMP/PS2/IRQ1/VGA are tested;
local GTK/physical keyboard interaction remains manual. Stop headless QEMU from
another terminal after the completion line. See [debugging](docs/debugging-guide.md).

First-fit and Best-fit reuse/split/coalesce aligned blocks over a Page Pool.
Allocation calls accept 1..4096 bytes, free retains idle arenas, and trim releases
pages and retries failed-acquire rollback records. Visible Revocation ACKs idle
resources; active/lost heaps fail-stop (96). Successful app status with failed
cleanup/leaks becomes 95. A check does not pin the next memory access.

[Independent LibOS reference](reference/independent-libos/README.md) is a different
route: its own startup/runtime/syscalls/fixed-object pool, no libael dependency.
Both can run as separate ELF modules/CR3s under the same kernel.

## Validate and maintain

```sh
make clean
make -j2
make profiles test sdk-tests app-sdk apps independent-libos
make package-tests
make integration LIBERUMS=../Liberums
make console-integration LIBERUMS=../Liberums
make phase4-integration LIBERUMS=../Liberums
make phase5-integration LIBERUMS=../Liberums
make phase6-integration LIBERUMS=../Liberums
```

The Phase 5 target retains all 27 real RAM revocation/audit guests. Phase 6 compiles
and runs packaged SDK consumers and every lesson. Historical ABI 1.1, ABI 1.2,
legacy ael_main, new ael_app_main, both policies/profiles and optional Console are
preserved. Six vendored ABI headers retain historical source commits and SHA256.

[Validation](docs/phase6-validation.md) · [Architecture](docs/architecture.md) ·
[Compatibility](docs/compatibility.md) · [Known limitations](docs/known-limitations.md) ·
[History](docs/history.md) · [Maintainer guide](docs/maintainer-guide.md) ·
[Changelog](CHANGELOG.md) · [Release notes](RELEASE-NOTES.md)

Official release-package requires a clean committed tree and emits source/SDK
archives, manifest, license, notes and SHA256SUMS. Packaging normalizes metadata
and SDK debug paths; cross-toolchain ELF identity is not promised. Generated
build artifacts are not committed. Maintenance continues in this original repo.

This is not a production-security or unlimited-uptime claim. Single-CPU i686,
static ELF32, bounded resources; no transparent pointer recovery, POSIX, network,
filesystem, spawn/IPC, dynamic linking, SMP, threads or GUI. No hour/day soak or
physical keyboard validation was performed. Phase 6 ends the planned handoff;
future architecture decisions belong to the maintainer.
