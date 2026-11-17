# SDK Console

GPL-3.0-or-later. Uses only public Application SDK 1.0 headers. Uses APP_DEVICES=1 and trusted VGA/keyboard grants; help/clear/about/status/memtest/exit.

From the source root use make app-console. For a completely independent project,
copy this directory and build with an installed SDK:

```sh
make AEL_SDK=/absolute/path/to/aelthuvor-sdk-1.0.0
```

APP_PROFILE and APP_POLICY select the runtime; the configuration stamp handles
changes. Output is build/app.elf by default. Runtime commands and grants are in
the SDK README and docs/build-guide.md; detailed lessons and error/lifetime
semantics are in docs/application-development and docs/sdk-reference.md.
