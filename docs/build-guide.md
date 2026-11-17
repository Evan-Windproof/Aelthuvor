# Build and package guide

Use Linux, GCC with i686 `-m32` code generation, GNU Binutils, GNU Make and
Python 3.12+ (archive extraction uses the data filter). Freestanding target code
does not require 32-bit host libc. Host tests require native ASan/UBSan and Linux
mmap/memfd. Integration needs QEMU system i386/TCG and GDB with i386/Python.
Use project and SDK paths without whitespace; installed Make rules do not escape
spaces in dependency filenames. Nothing downloads a toolchain automatically.

```sh
git clone https://github.com/Evan-Windproof/Aelthuvor.git
cd Aelthuvor
git checkout v1.0.0
make -j2
make profiles app-sdk apps independent-libos
make sdk-package
```

`make app-sdk` exports build/app-sdk with headers, libraries, four runtimes,
crt0.o, app-entry.o, app.mk, linker.ld, VERSION, README, license and public-header
provenance. `make sdk-package` adds template, example sources and developer docs,
BUILD-INFO.json, SDK-FILES.json and a deterministic SDK tarball in build/release.
Dirty development builds are labeled; only `make release-package` from a clean
committed tree produces the official source bundle and clean release manifest.
Source bundles include SOURCE.json with the source commit, epoch and file hashes,
so SDK packaging also works after extraction without a .git directory. Modified
source files are labeled dirty and cannot produce an official release bundle.

Verify downloaded artifacts before extracting:

```sh
sha256sum -c SHA256SUMS
tar -xzf aelthuvor-sdk-1.0.0.tar.gz
export SDK="$(pwd)/aelthuvor-sdk-1.0.0"
cp -R "$SDK/templates/application" my-app
make -C my-app AEL_SDK="$SDK"
make -C my-app AEL_SDK="$SDK" APP_POLICY=best
```

Keep the compiler/assembler/linker prefix consistent with `CROSS_COMPILE=`.
Selectors are APP_PROFILE=preempt|coop, APP_POLICY=first|best, APP_DEVICES=0|1.
OUT and SOURCES are application-local. Changing selectors updates the .config
stamp and rebuilds application objects, selecting the corresponding runtime.
No Linux main, libc, dynamic linker or private kernel includes are needed.

Build the independent kernel once in its own checkout, pinned at
`7d5d574e8643c3645fa3ce2a126dcbd4ae43662f` (Liberums v1.0.0):

```sh
make -C /path/to/Liberums -j2
export KERNEL=/path/to/Liberums/build/liberums.elf
qemu-system-i386 -machine pc -accel tcg -cpu qemu32 -m 32M -smp 1 \
  -display none -serial stdio -monitor none -nic none -no-reboot -no-shutdown \
  -kernel "$KERNEL" -append multi-external -initrd my-app/build/app.elf
```

Cooperative ELF uses `-append external`; the multi launcher requires preemption.
Console uses `-append 'multi-external multi-vga=1 multi-keyboard=1'`; granting
domain 1 is a trusted boot choice, never an application API. With headless
`-monitor none`, terminate QEMU from another terminal after the completion line.
For a graphical local console, replace `-display none` with `-display gtk` when
your QEMU supports GTK and a graphical session is available. Physical keyboard
and GTK focus still require manual local validation.

`make test sdk-tests package-tests` checks native invariants and real compiler /
linker consumers. The [validation record](phase6-validation.md) lists all QEMU
targets. App development does not rebuild the kernel: only integration setup may
invoke its existing Make targets to prepare an independent reference binary.

Archive order, uid/gid, modes and timestamps are normalized. SOURCE_DATE_EPOCH
defaults to the source commit time. GCC file/debug prefix maps remove checkout
paths from SDK objects. Identical archives are tested for the same built inputs;
ELF byte identity across different toolchains/platforms is not promised.
