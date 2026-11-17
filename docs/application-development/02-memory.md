# Lesson 02 — Memory allocation and OOM

## Goal

Allocate aligned memory, check errors, and clean up a bounded quota experiment.

## Example and explanation

[Complete compilable source](../../examples/tutorial/02-memory.c). The focused fragment below belongs inside ael_app_main unless it includes the full entry:

```c
unsigned char *p = ael_app_alloc(ctx, 128);
if (!p) { return 1; }
p[0] = 42;
if (ael_app_free(ctx, p) || ael_app_trim(ctx)) { return 2; }
```

Allocations accept 1..4096 bytes and align to 16 bytes. The full source verifies all 128 bytes, then holds full-page objects until the quota/OOM boundary. It records last-error before cleanup, releases every successful allocation and trims. ENOSPC and ENOMEM are ordinary failures, not valid pointers.

## Build and run

Start with the SDK, KERNEL and run_app setup in the [course introduction](README.md). Use the copied tutorial project directory.

```sh
make AEL_SDK="$SDK" LESSON=02-memory OUT=build/02-memory
run_app build/02-memory/app.elf
```

## Common errors

Read ael_app_error immediately after NULL; later successful calls reset it. Free only exact live starts. Free does not immediately release its arena.

## Acceptance

The bounded loop reaches ENOSPC/ENOMEM and returns 0 after cleanup; final kernel resource count is zero.
