# Lesson 01 — Hello Aelthuvor

## Goal

Implement the SDK entry and inspect its two version systems.

## Example and explanation

[Complete compilable source](../../examples/tutorial/01-hello.c). The focused fragment below belongs inside ael_app_main unless it includes the full entry:

```c
struct ael_app_info info;
if (ael_app_get_info(ctx, &info)) { return 1; }
return ael_app_log(ctx, "Hello Aelthuvor\n") ? 2 : 0;
```

The SDK owns startup and supplies ctx. The full example checks Application API 1.0 and Kernel ABI 1.2 independently, then writes a short diagnostic message. A zero return asks the adapter to clean up and exit.

## Build and run

Start with the SDK, KERNEL and run_app setup in the [course introduction](README.md). Use the copied tutorial project directory.

```sh
make AEL_SDK="$SDK" LESSON=01-hello OUT=build/01-hello
run_app build/01-hello/app.elf
```

## Common errors

Defining main instead of ael_app_main leaves the adapter entry unresolved. A long log can hit the kernel output quota; check its return.

## Acceptance

The serial log contains the greeting and the kernel completion line; GDB confirms actual CPL3 and exit status 0.
