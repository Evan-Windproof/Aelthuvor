# Ten SDK tutorial programs

GPL-3.0-or-later. Each C file defines ael_app_main and uses only public app.h or
app_console.h. Copy this directory outside the repository, then:

```sh
make AEL_SDK=/path/to/aelthuvor-sdk-1.0.0 LESSON=01-hello
make AEL_SDK=/path/to/aelthuvor-sdk-1.0.0 LESSON=03-policy APP_POLICY=best
make AEL_SDK=/path/to/aelthuvor-sdk-1.0.0 LESSON=10-project APP_DEVICES=1
```

LESSON defaults to 01-hello. Console Lesson 05 selects APP_DEVICES=1; project
Lesson 10 is headless unless requested. Use distinct OUT directories or let the
configuration stamp rebuild after switching. Complete explanations and runtime
commands are in docs/application-development inside the SDK package/source tree.
