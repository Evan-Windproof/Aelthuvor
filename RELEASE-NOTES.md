# Aelthuvor v1.0.0 — Library OS & Application SDK Release

This release establishes an educational and developer baseline for an independent
Ring 3 LibOS on Liberums v1.0.0. Kernel protects resources; LibOS manages resources.
Aelthuvor is optional: another LibOS can use the kernel without linking it.

Application API remains **1.0**, Kernel ABI remains **1.2**. Existing ael_main,
new opaque ael_app_main, cooperative/preemptive runtimes, first/best allocation,
optional VGA/PS2 Console and independent LibOS reference are retained. No Runtime,
Heap or device redesign was needed for this release.

New deliverables are a portable SDK package, ten runnable lessons, explicit API /
error/lifetime documentation, three README languages and maintainer instructions.
SDK objects normalize checkout paths. Packages normalize archive metadata and
record source commit, dirty state, toolchain, member hashes and public ABI origin.
The final official manifest records the exact tag/source commit; binaries are not
claimed identical across different GCC/Binutils/platforms.

Assets: aelthuvor-sdk-1.0.0.tar.gz, aelthuvor-1.0.0-source.tar.gz,
release-manifest.json, SHA256SUMS, this note and LICENSE. The SDK includes headers,
two static libraries, crt0/app-entry, four profile/policy runtimes, linker/build
rules, template, examples and developer documentation. No kernel, VM, cache,
test logs or unrelated prebuilt application ELF is bundled.

Validate the downloaded asset set with `sha256sum -c SHA256SUMS`. Extract the SDK,
copy templates/application into a new project, and build with
`make AEL_SDK=/absolute/path/to/aelthuvor-sdk-1.0.0`. See docs/application-development
and docs/build-guide.md. The separate kernel pin is
`7d5d574e8643c3645fa3ce2a126dcbd4ae43662f`.

Validation details are in docs/phase6-validation.md. Host sanitizers, build/link
audits, real QEMU/GDB guests and completed domain lifetimes are counted separately.
All historical Phase 1–5 targets, including the 27 Phase 5 revocation guests,
remain part of final acceptance. The package consumer tests compile and edit an
application outside both repositories, execute the changed ELF, and require
unchanged SDK libraries and kernel SHA256/mtime.

Limits: single-CPU i686, static ELF32, bounded quotas, no libc/POSIX/filesystem /
network/spawn/IPC/SMP/threads or GUI. Active revocation uses Fail-stop (96);
ordinary cleanup/leak failure after successful application status uses 95.
Checkpoints cannot pin the next load/store; old pointers may fault or alias a
later allocation. No production-security, unlimited-uptime, physical keyboard or
GTK validation claim is made. See docs/known-limitations.md.

GPL-3.0-or-later; original per-header Liberums provenance is retained. Maintenance
continues in Evan-Windproof/Aelthuvor with docs/maintainer-guide.md. A tag denotes
the source baseline; confirm the separate GitHub Release and assets before
claiming publication. No next development phase is started automatically.
