# Application development course

Ten small C programs introduce the released Application SDK 1.0. They require
only the SDK, a freestanding compiler/linker and a separately built compatible
Liberums v1.0.0 kernel. The source checkout is not an application build dependency.

Verify SHA256SUMS and extract the SDK using the [build guide](../build-guide.md).
Use absolute paths without spaces for SDK and KERNEL. Copy only tutorial sources
and their Makefile into your own project:

```sh
export SDK=/absolute/path/to/aelthuvor-sdk-1.0.0
export KERNEL=/absolute/path/to/Liberums/build/liberums.elf
mkdir my-lessons
cp "$SDK/examples/tutorial/"*.c "$SDK/examples/tutorial/Makefile" my-lessons/
cd my-lessons
run_app () {
  qemu-system-i386 -machine pc -accel tcg -cpu qemu32 -m 32M -smp 1 \
    -display none -serial stdio -monitor none -nic none -no-reboot -no-shutdown \
    -kernel "$KERNEL" -append "multi-external ${2:-}" -initrd "$1"
}
```

Run commands in the same shell so the function/variables remain available.
Headless tests print completion but keep QEMU alive with -no-shutdown; terminate
that process from another terminal afterward. For manual Console interaction,
replace -display none with -display gtk on a local GTK-enabled QEMU and desktop.
Cloud acceptance uses QMP/PS2/IRQ1 and actual VGA memory, not a claim of physical
keyboard or GTK testing. Cooperative SDK variants instead use -append external.

| Lesson | Subject |
| --- | --- |
| [01 — Hello Aelthuvor](01-hello.md) | Implement the SDK entry and inspect its two version systems. |
| [02 — Memory allocation and OOM](02-memory.md) | Allocate aligned memory, check errors, and clean up a bounded quota experiment. |
| [03 — First-fit and Best-fit](03-policy.md) | Observe a policy decision without changing application API or kernel code. |
| [04 — Application Context](04-context.md) | Understand a borrowed opaque context and last-error lifetime. |
| [05 — Optional Console service](05-console.md) | Request existing VGA/PS2 grants, read a line and close both devices. |
| [06 — Resource lifecycle](06-lifecycle.md) | Distinguish allocation free, arena trim, capability release and domain exit. |
| [07 — Revocation and Fail-stop](07-revocation.md) | Learn the difference between a checkpoint, a pin and safe ownership retirement. |
| [08 — Build and debug](08-debug.md) | Use ELF symbols, link maps and hardware breakpoints to observe real Ring 3 state. |
| [09 — Independent application from an empty directory](09-independent.md) | Build an ordinary Aelthuvor application without either repository private source. |
| [10 — Create your own project](10-project.md) | Combine memory management, verified computation, errors and optional interaction. |

Every source is compiled from the packaged SDK and executed by the Phase 6
integration target. Lesson 3 runs first and best; Lesson 10 runs headless and
with Console. The revocation sample is a normal checkpoint example: the
separate unchanged Phase 5 suite supplies real ACK/force/#PF evidence.

When starting from the source tree, run make sdk-package first. Maintainers can
run make package-tests and make phase6-integration LIBERUMS=/path/to/Liberums.
[SDK reference](../sdk-reference.md) specifies all returns and lifetime limits;
[known limitations](../known-limitations.md) bounds the release claims.
