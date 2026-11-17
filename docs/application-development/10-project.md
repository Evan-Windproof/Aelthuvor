# Lesson 10 — Create your own project

## Goal

Combine memory management, verified computation, errors and optional interaction.

## Example and explanation

[Complete compilable source](../../examples/tutorial/10-project.c). The focused fragment below belongs inside ael_app_main unless it includes the full entry:

```c
uint32_t *values = ael_app_alloc(ctx, 128 * sizeof *values);
if (!values) { return 1; }
/* Fill 0..127 and sum to 8128. */
if (ael_app_free(ctx, values) || ael_app_trim(ctx)) { return 2; }
```

The full project fills 128 integers, computes and verifies 8128, then releases memory before normal return. A build-time APP_DEVICES=1 variant opens Console, reads a label, prints the sum and closes devices. The headless form has no VGA/keyboard symbols or grants. Keep error handling explicit and the program bounded.

## Build and run

Start with the SDK, KERNEL and run_app setup in the [course introduction](README.md). Use the copied tutorial project directory.

```sh
make AEL_SDK="$SDK" LESSON=10-project OUT=build/10-project
run_app build/10-project/app.elf
```

```sh
make AEL_SDK="$SDK" LESSON=10-project OUT=build/project-console APP_DEVICES=1
run_app build/project-console/app.elf "multi-vga=1 multi-keyboard=1"
```

## Common errors

Do not let successful main hide leaked allocations (95), assume devices are available, or mistake optional linkage for permission. New architecture features belong to maintainer decisions, not this exercise.

## Acceptance

Headless serial reports checksum 8128 and exit 0. The optional form accepts hello through real PS2 input and closes both devices. Both variants are package-only Phase 6 guest cases.
