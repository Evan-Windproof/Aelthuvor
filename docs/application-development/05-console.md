# Lesson 05 — Optional Console service

## Goal

Request existing VGA/PS2 grants, read a line and close both devices.

## Example and explanation

[Complete compilable source](../../examples/tutorial/05-console.c). The focused fragment below belongs inside ael_app_main unless it includes the full entry:

```c
if (ael_app_console_open(ctx)) { return 1; }
int length = ael_app_readline(ctx, lesson_line, sizeof lesson_line);
if (length < 0) { return 2; }
if (ael_app_console_close(ctx)) { return 3; }
```

APP_DEVICES=1 links the optional archive but grants no authority. Trusted boot grants VGA/keyboard to domain 1. The complete example prints a prompt, handles queue-overflow separately, echoes the line and closes explicitly. Other error returns still reach adapter cleanup.

## Build and run

Start with the SDK, KERNEL and run_app setup in the [course introduction](README.md). Use the copied tutorial project directory.

```sh
make AEL_SDK="$SDK" LESSON=05-console OUT=build/05-console
run_app build/05-console/app.elf "multi-vga=1 multi-keyboard=1"
```

## Common errors

Missing boot grants return EPERM. A caller buffer retains capacity-1 characters; keyboard queue loss instead returns EOVERFLOW and clears the line/decoder. Device loss stops the service. Readline is blocking and single-threaded.

## Acceptance

Type hello then Enter in a local graphical console. Automated acceptance injects real QMP/PS2 input, observes IRQ1, checks the input bytes and VGA RAM, then verifies device close and domain teardown.
