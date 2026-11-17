# Lesson 07 — Revocation and Fail-stop

## Goal

Learn the difference between a checkpoint, a pin and safe ownership retirement.

## Example and explanation

[Complete compilable source](../../examples/tutorial/07-revocation.c). The focused fragment below belongs inside ael_app_main unless it includes the full entry:

```c
if (ael_app_check(ctx)) { return 1; }
/* This is not a lease: a later forced revoke may still fault. */
p[0] = 42;
```

Visible Revocation uses non-consuming POLL, INFO fields and ACK of the full index/generation/request identity. An idle arena can ACK; an active arena retains allocation evidence and stops with 96. Successful main with cleanup errors/leaks instead exits 95. Forced completion can remove the event, so the client also queries handles. Old pointers cannot be transparently restored.

## Build and run

Start with the SDK, KERNEL and run_app setup in the [course introduction](README.md). Use the copied tutorial project directory.

```sh
make AEL_SDK="$SDK" LESSON=07-revocation OUT=build/07-revocation
run_app build/07-revocation/app.elf
```

## Common errors

Do not catch 96 and keep using the pointer, ACK active memory, or treat check as a guarantee for the next load. The sample normally has no injected event; it does not itself prove forced revocation.

## Acceptance

Run the sample for normal exit. In a source checkout run make phase5-integration LIBERUMS=/path/to/Liberums: its 27 actual QEMU/GDB cases include idle ACK, real PIT force, stale handles, zeroing, exit 96, expected #PF and independent-domain survival.
