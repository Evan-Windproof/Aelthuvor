# 独立构建应用

初次构建 SDK 与示例：

```sh
make app-sdk
make apps                  # Hello / Memory 两策略 / SDK Console / 外部模板
make app-hello
make app-memory
make app-console           # 新公共 API Console
make console               # 保留 Phase 3 兼容 Console
make sdk-tests             # 导出/隔离构建/改源码/入口冲突/依赖审计
```

导出目录 build/app-sdk 可复制到其他项目，包含 include、lib、linker.ld、app.mk、LICENSE 和内核 SDK 来源记录。
消费者只需 GCC/Binutils/Make 及普通 Linux 构建工具，不需要 Aelthuvor src 或 Liberums checkout。
SDK 本身的构建/导出使用 Python。链接无宿主 libc、无动态链接器。

```sh
make -C examples/application-template AEL_SDK=/absolute/path/to/app-sdk
# 另一个位置也可：复制 main.c/Makefile/README.md 到新目录后执行相同命令
make AEL_SDK=/absolute/path/to/app-sdk
```

这个目录是 **Aelthuvor 应用模板**，不是通用 Liberums LibOS 模板。
它只实现 ael_app_main，不直接处理 syscall、资源句柄或内核头。

| 构建设置 | 值 |
| --- | --- |
| APP_PROFILE | preempt（默认）或 coop，选择正确 note/runtime |
| APP_POLICY | first（默认）或 best |
| APP_DEVICES | 0（默认）或 1，设备库可选；是否有 grant 仍由可信启动决定 |
| SOURCES | 应用自己的 C 文件列表，默认 main.c |
| OUT | 应用输出目录，默认 build |

app.mk 的配置记录保证在同一 OUT 切换 profile/policy/devices 时重建相关应用，不误用旧 runtime。
源码依赖由 -MMD 记录。修改 main.c 只编译应用对象和链接应用，不重编译库或内核。
构建测试会在隔离目录实际修改模板欢迎文字，核对 ELF 改变、库/对象 hash 与 mtime 不变；
再切换 first→best→first 验证配置选择，并将真正独立生成的 ELF 交给 QEMU 执行。

静态链接顺序是应用对象、crt0、选定 runtime、单一 adapter、可选设备 archive、core archive。
archive 只提取被引用的对象；这就是不含设备功能的程序无需设备模块的原因。
低层 runtime 继续初始化原来的 allocator；API 不替换其策略。
