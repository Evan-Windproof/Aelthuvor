# Lesson 08 — Build and debug

## Goal

Use ELF symbols, link maps and hardware breakpoints to observe real Ring 3 state.

## Example and explanation

[Complete compilable source](../../examples/tutorial/08-debug.c). The focused fragment below belongs inside ael_app_main unless it includes the full entry:

```c
lesson_pointer = ael_app_alloc(ctx, 4096);
for (unsigned i = 0; i < 1024; ++i) { lesson_pointer[i] = 0xa6e10000u | i; }
lesson_observed(1);
```

The complete source exports lesson_observed, lesson_stage and lesson_pointer without private LibOS headers. Stage 1 holds a filled page; stage 2 follows free/trim and pointer clearing. The linker emits app.map and debug symbols. In a multi-domain guest the same virtual PC may map another ELF, so filter by domain and inspect CR3.

## Build and run

Start with the SDK, KERNEL and run_app setup in the [course introduction](README.md). Use the copied tutorial project directory.

```sh
make AEL_SDK="$SDK" LESSON=08-debug OUT=build/08-debug
run_app build/08-debug/app.elf
```

For an interactive debug stop, follow [the GDB session](../debugging-guide.md), use this lesson ELF and `hbreak *lesson_observed`. At entry the stage argument is at `$esp+4`; the global stage is assigned inside the function.

## Common errors

Do not use Linux run or host libc. A syscall breakpoint stops in Ring 0: inspect its saved user frame, not current CS alone. Delete hardware breakpoints as only four slots exist.

## Acceptance

The Phase 6 GDB audit checks user CS/SS/CR3, actual PTE flags, all 1024 words, owned/live counts, then absent PTE and physical zero/free state. See ../debugging-guide.md for an interactive session.
