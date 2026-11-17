# Aelthuvor Application SDK 1.0

Release version: see VERSION. Target: C11/i686 freestanding ELF32, GPL-3.0-or-later.
Compatible kernel: Liberums v1.0.0, Kernel ABI 1.2. Kernel ABI and Application API
are separate version systems. No Linux libc, dynamic loader or kernel objects.

Copy `templates/application/` from the SDK package to your own project directory:

```sh
make AEL_SDK=/absolute/path/to/aelthuvor-sdk-1.0.0
make AEL_SDK=/absolute/path/to/aelthuvor-sdk-1.0.0 APP_POLICY=best
```

For a raw `make app-sdk` export, copy the source repository's
`examples/application-template/` separately. Only `include/aelthuvor/app.h` and
optional `app_console.h` are the ordinary Application API 1.0 contract. Other
Aelthuvor headers are advanced LibOS interfaces, not a stable internal layout ABI.

Implement `int ael_app_main(struct ael_app_context *context)`. The borrowed opaque
context is valid during that invocation only. Return 0 after releasing allocations,
or return an application error. Never define `main`, `_start` or a second
`ael_main` when using this adapter. Core archives contain no application entry.

Build selectors: `APP_PROFILE=preempt|coop`, `APP_POLICY=first|best`,
`APP_DEVICES=0|1`, `OUT=build`, `SOURCES=main.c`, optional `CROSS_COMPILE`.
Default: preempt/first/no devices. The installed app.mk chooses exactly one
runtime and explicit crt0/app-entry objects. Configuration changes invalidate
application objects. Device linking requests no authority: trusted kernel boot
must grant VGA and keyboard to the selected domain.

Launch preemptive applications with a separately built Liberums kernel:

```sh
qemu-system-i386 -machine pc -accel tcg -cpu qemu32 -m 32M -smp 1 \
  -display none -serial stdio -monitor none -nic none -no-reboot -no-shutdown \
  -kernel /path/to/Liberums/build/liberums.elf -append multi-external \
  -initrd build/app.elf
```

Cooperative applications use the kernel's `external` boot path; `multi-external`
requires preemption. Console additionally needs `multi-vga=1 multi-keyboard=1`.
Stop headless QEMU after its completion message; `-no-shutdown` keeps it alive.

Allocation is 16-byte aligned, 1..4096 bytes per call. NULL means failure; inspect
`ael_app_error()` immediately. Invalid/interior/double free returns EINVAL.
Free leaves idle arenas; Trim returns pages. A successful main with leaked
allocations/cleanup errors exits 95. An unsafe pending/lost heap exits 96.
Checkpoints do not pin memory; a subsequent forced revoke can still cause #PF.
There is no transparent migration or safe resurrection of old pointers.

Package contents include `lib/`, `include/`, app.mk, linker.ld, source provenance,
license, template, example sources and developer documentation. `BUILD-INFO.json`
records the build source commit, dirty state and toolchain. `SDK-FILES.json`
records member hashes; verify the outer archive using the release SHA256SUMS.
Development packages can be marked dirty; official release-package requires a
clean committed tree. Fixed-commit packaging normalizes tar metadata and gzip
timestamps. Cross-toolchain ELF byte identity is not promised.
