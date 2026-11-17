# v1.0.0 compatibility matrix

| Component | Supported baseline / evidence |
| --- | --- |
| Aelthuvor release | v1.0.0; source commit recorded in release manifest |
| Application API | 1.0 (`0x00010000`), opaque app.h and optional app_console.h |
| Kernel | Liberums v1.0.0, `7d5d574e8643c3645fa3ce2a126dcbd4ae43662f` |
| Kernel ABI | 1.2; six public headers match fixed provenance and v1.0.0 bytes |
| Historical ABI | Original ABI 1.1 ELF rebuilt from Phase 1 and tested without Note edits |
| Profiles | coop and preempt; distinct runtime objects and compatible launcher required |
| Policies | first and best, same Application API, fragmentation/pressure invariants |
| Entrypoints | legacy ael_main(heap), new ael_app_main(context); choose one explicit adapter |
| Devices | Optional VGA/PS2 library, existing boot grants, no grant from linkage |
| Other LibOS | Independent reference links no ael library; coexistence and fault isolation tested |

Core headers elf.h/resource.h/syscall.h/event.h retain source commit
`85a493466afe2038ca22a9870f6f9ce37dd7f3a8`; vga.h/keyboard.h retain
`5d9c8f35a4ae54ef08b93b7f4d5fecd9e350acbf`. Per-file SHA256 and repository URLs
remain in SDK-PROVENANCE.json (sdk/manifest.json in source). Sources keep their
GPL-3.0-or-later license. No internal Liberums header or object is distributed
inside the SDK. A kernel release number does not replace actual header provenance.

See [validation](phase6-validation.md), [API reference](sdk-reference.md) and
[limitations](known-limitations.md). Future kernel releases require a fresh
compatibility review; numeric version similarity alone is insufficient.
