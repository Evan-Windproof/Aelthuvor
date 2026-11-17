# Lesson 06 — Resource lifecycle

## Goal

Distinguish allocation free, arena trim, capability release and domain exit.

## Example and explanation

[Complete compilable source](../../examples/tutorial/06-lifecycle.c). The focused fragment below belongs inside ael_app_main unless it includes the full entry:

```c
if (ael_app_free(ctx, p)) { return 1; }
if (ael_app_get_info(ctx, &info)) { return 2; }
/* live_allocations == 0; the idle page is still owned. */
if (ael_app_trim(ctx)) { return 3; }
```

The example checks live=1/owned=1, then live=0/owned=1 after free, then both zero after trim. Page Pool owns capabilities; Heap owns block records. Advanced LibOS authors may call page claim/map/unmap/release, but ordinary SDK apps should leave this coordination to alloc/free/trim. UNMAP removes a binding, not ownership; RELEASE removes all aliases and returns the resource.

## Build and run

Start with the SDK, KERNEL and run_app setup in the [course introduction](README.md). Use the copied tutorial project directory.

```sh
make AEL_SDK="$SDK" LESSON=06-lifecycle OUT=build/06-lifecycle
run_app build/06-lifecycle/app.elf
```

## Common errors

Releasing a heap backing page through raw ABI bypasses live-object policy. Do not mix raw page calls with SDK-owned arenas. Failed rollback records are retained and retried by trim.

## Acceptance

All three info snapshots match, main exits 0, and final physical/resource accounting returns to baseline.
