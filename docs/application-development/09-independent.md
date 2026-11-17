# Lesson 09 — Independent application from an empty directory

## Goal

Build an ordinary Aelthuvor application without either repository private source.

## Example and explanation

[Complete compilable source](../../examples/tutorial/09-independent.c). The focused fragment below belongs inside ael_app_main unless it includes the full entry:

```c
#include <aelthuvor/app.h>
int ael_app_main(struct ael_app_context *ctx)
{
    return ael_app_log(ctx, "My independent application\n") ? 1 : 0;
}
```

Create a new directory, place this main.c inside, and write a Makefile containing include $(AEL_SDK)/app.mk. The SDK supplies startup, runtime, entry adapter and linker script. You can also copy templates/application from the package. An Ael SDK application differs from creating another LibOS: the separate independent-libos reference does not link ael at all.

## Build and run

Start with the SDK, KERNEL and run_app setup in the [course introduction](README.md). Use the copied tutorial project directory.

```sh
make AEL_SDK="$SDK" LESSON=09-independent OUT=build/09-independent
run_app build/09-independent/app.elf
```

```sh
mkdir ../my-independent
cd ../my-independent
# Save the complete minimal C example above as main.c.
printf '%s\n' 'include $(AEL_SDK)/app.mk' > Makefile
make AEL_SDK="$SDK"
run_app build/app.elf
```

## Common errors

Compiler dependencies must point only to your files, system freestanding headers and SDK/include. A reference to src/app_internal.h or kernel/ is a boundary violation. An SDK app ELF is not a Linux program.

## Acceptance

Build, change the message in C, rebuild and run the changed ELF. The new ELF hash differs while SDK library hashes/mtimes and the already built kernel remain unchanged. Phase 6 automates this actual external-consumer experiment.
