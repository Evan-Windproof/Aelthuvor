# Lesson 03 — First-fit and Best-fit

## Goal

Observe a policy decision without changing application API or kernel code.

## Example and explanation

[Complete compilable source](../../examples/tutorial/03-policy.c). The focused fragment below belongs inside ael_app_main unless it includes the full entry:

```c
uintptr_t first = (uintptr_t)blocks[1];
uintptr_t best = (uintptr_t)blocks[3];
/* Free 256-byte and 128-byte holes, separated by live blocks. */
void *chosen = ael_app_alloc(ctx, 96);
```

The complete experiment allocates 64,256,64,128,64 bytes, frees the two holes and requests 96. First-fit chooses the earlier 256-byte hole; Best-fit chooses the smaller 128-byte hole. The guards prevent coalescing. Both reuse the existing page. This one trace demonstrates selection, not a universal memory-efficiency ranking.

## Build and run

Start with the SDK, KERNEL and run_app setup in the [course introduction](README.md). Use the copied tutorial project directory.

```sh
make AEL_SDK="$SDK" LESSON=03-policy OUT=build/03-policy
run_app build/03-policy/app.elf
```

```sh
make AEL_SDK="$SDK" LESSON=03-policy OUT=build/policy-best APP_POLICY=best
run_app build/policy-best/app.elf
```

## Common errors

Comparing holes after trim/address reuse proves nothing about old pointer identity. Keep separator blocks live and store numeric addresses before free.

## Acceptance

Build and run both policies. Each verifies the chosen address against info.policy and exits 0; neither changes the kernel.
