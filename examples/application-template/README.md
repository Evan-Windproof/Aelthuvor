# Aelthuvor Application Template

这是使用 Aelthuvor Application API 的普通应用，不是通用 Liberums LibOS 模板。
许可证 GPL-3.0-or-later。仅包含公开 app.h，不引用 src、内核私有头、ael_runtime 或内部 Heap。

先在 Aelthuvor 构建一次 `make app-sdk`，将 build/app-sdk 复制到任意安装目录。
把本目录复制到自己的项目后：

```sh
make AEL_SDK=/absolute/path/to/app-sdk
make AEL_SDK=/absolute/path/to/app-sdk APP_POLICY=best
```

只需要 GCC、Binutils、GNU Make 和标准 Linux 工具；目标是 freestanding C11/i686 ELF32，不是 Linux 可执行程序。
默认 OUT=build、APP_PROFILE=preempt、APP_POLICY=first、APP_DEVICES=0。
实现 int ael_app_main(struct ael_app_context *ctx)，使用快照、alloc/free/trim/log。
上下文由 SDK 绑定，普通错误检查负数返回，严重失效不会继续返回旧指针。

运行 build/app.elf 需要兼容内核的 external preempt 或 multi-external。没有 VGA/Keyboard 权限依赖。
更完整说明见 docs/phase4-application-api.md 和 phase4-independent-apps.md。
