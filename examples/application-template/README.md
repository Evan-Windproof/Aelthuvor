# Standalone Application SDK template

GPL-3.0-or-later. This is an Aelthuvor application, not a template for another
LibOS. It includes only public app.h and implements ael_app_main. It does not
read app_internal.h, ael_runtime, Heap layouts or private kernel headers.

Copy this directory to your own project and set the installed SDK explicitly:

```sh
make AEL_SDK=/absolute/path/to/aelthuvor-sdk-1.0.0
make AEL_SDK=/absolute/path/to/aelthuvor-sdk-1.0.0 APP_POLICY=best
```

Defaults: OUT=build, APP_PROFILE=preempt, APP_POLICY=first, APP_DEVICES=0.
The source performs an 80-byte data check, free/trim and a short log. Edit the
message and rebuild; no static SDK library or kernel rebuild is necessary.
The output is a freestanding ELF32 guest, not a Linux executable.

Run using the SDK README/build guide: multi-external for preemptive ELF, external
for cooperative ELF. Console is optional and requires both linkage and trusted
boot grants. Core-only templates require neither VGA nor keyboard.

Full lessons are in docs/application-development in the source and SDK package.
API errors and pointer lifetime are specified in docs/sdk-reference.md. Free
relinquishes a block; trim returns idle pages. Unsafe heap exits 96; cleanup
errors/leaks after otherwise successful main produce 95.
