# Lesson 04 — Application Context

## Goal

Understand a borrowed opaque context and last-error lifetime.

## Example and explanation

[Complete compilable source](../../examples/tutorial/04-context.c). The focused fragment below belongs inside ael_app_main unless it includes the full entry:

```c
if (ael_app_alloc(ctx, 0) != 0) { return 1; }
if (ael_app_error(ctx) != AEL_APP_EINVAL) { return 2; }
if (ael_app_check(ctx)) { return 3; }
```

Do not define or allocate struct ael_app_context. It belongs to the SDK for this invocation. A successful check clears last-error; get_info is also a checked operation. Return ends the context through an internal adapter. There is no public bind/finish call to invoke manually.

## Build and run

Start with the SDK, KERNEL and run_app setup in the [course introduction](README.md). Use the copied tutorial project directory.

```sh
make AEL_SDK="$SDK" LESSON=04-context OUT=build/04-context
run_app build/04-context/app.elf
```

## Common errors

Including app_internal.h defeats the public boundary and is unsupported. Never save ctx for use after main or exit; calling exit never returns.

## Acceptance

The example observes EINVAL immediately, then 0 after a successful check. The host suite additionally checks all closed-context and unsafe-heap gates.
