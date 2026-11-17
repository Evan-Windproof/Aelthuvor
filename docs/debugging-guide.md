# Debugging ELF32 applications

First inspect the application, not a Linux process image:

```sh
readelf -h build/app.elf
readelf -l build/app.elf
readelf -n build/app.elf
nm -u build/app.elf
less build/app.map
```

Expect ELF32 / Intel 80386 / EXEC, _start, RX/R/RW segments, LIBERUMS ABI Note
and no undefined symbols. Link maps show the explicit runtime/entry and selected
libraries. No device symbols should appear in a core-only Hello. A duplicate
ael_main usually means the old and new entry adapters were linked together.
Undefined printf/malloc/main means host-libc assumptions entered freestanding code.

Launch a debug guest (use a free TCP port):

```sh
qemu-system-i386 -machine pc -accel tcg -cpu qemu32 -m 32M -smp 1 \
  -display none -serial file:serial.log -monitor none -nic none \
  -no-reboot -no-shutdown -S -gdb tcp:127.0.0.1:1234 \
  -kernel "$KERNEL" -append multi-external -initrd build/app.elf
gdb /path/to/Liberums/build/liberums.elf
```

In GDB:

```gdb
set architecture i386
target remote 127.0.0.1:1234
add-symbol-file build/app.elf
hbreak *ael_app_main
continue
info registers cs ss cr3 eflags eip esp
hbreak *syscall_dispatch
continue
```

Use hardware breakpoints: software INT3 modifies the mapped instruction bytes.
The loader uses separate address spaces at identical virtual addresses. Filter
breakpoints by current_domain->id in multi-domain tests and never assume a
linear PC uniquely identifies an ELF. Only four hardware slots are available;
delete old breakpoints before adding more. At user entry CS=0x1b, SS=0x23,
IOPL=0 and a private CR3 are expected. At syscall_dispatch CS is kernel CS;
inspect the saved trap frame to establish the caller's CPL3 and scalar registers.
CR3 selects the page directory; it is not a resource handle or physical payload.

Lesson 8 exports lesson_observed, lesson_stage and lesson_pointer. Stop on its
stage 1 call to inspect 1024 words of `0xa6e10000 | i`, then stage 2 after trim.
Read only through the correct active CR3 or explicitly walk its PTEs. Paths in
DWARF are normalized: use `directory /path/to/your/project` and
`directory /path/to/Aelthuvor` to locate sources, or GDB substitute-path as needed.

For reproducible machine-state audits, run phase4/phase5/phase6 integration targets
with `QEMU=/path/to/qemu-system-i386 GDB=/path/to/gdb`. Generated scripts and
logs identify the actual instructions, PTEs, physical pages, IRQs and teardown.
The trusted revocation mailbox is diagnostic kernel machinery, not a production
application syscall or permission to patch PTEs. See Lesson 7 for real revoke
tests. Serial text alone cannot prove isolation or reclamation.

Interpret exit 93 as ABI/runtime probe incompatibility, 94 as heap initialization
failure, 95 as failed cleanup/leaked live allocation after otherwise successful
exit, and 96 as unsafe heap Fail-stop. Application-selected nonzero returns are
otherwise preserved. A forced revoke may cause #PF before the next API check;
record vector, error, CR2 and the exact EIP, then confirm peer-domain completion.
