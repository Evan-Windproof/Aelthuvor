# Aelthuvor — 独立 Library OS 与 Application SDK

[English](README.md) · [繁體中文](README.zh-TW.md)

Aelthuvor 是运行于 Liberums 上的 C11/i686 Ring 3 Library OS。
**内核保护资源，LibOS 管理资源。** 它不是内核的必需组成部分，其他 LibOS 可以完全不使用它。
许可证 GPL-3.0-or-later。

v1.0.0 是教学与开发基线：**Application API 1.0**，兼容 **Liberums v1.0.0 / Kernel ABI 1.2**，
内核固定提交 `7d5d574e8643c3645fa3ce2a126dcbd4ae43662f`。两个版本体系独立。
Phase 6 完成 SDK 打包、开发课程与维护交接。源码 Tag 不等于 GitHub Release；
托管资产状态以[实际发布页](https://github.com/Evan-Windproof/Aelthuvor/releases)为准。

## 构建与开发

需要 Linux、支持 -m32/i686 的 GCC、GNU Binutils/Make、Python 3.12+。
目标为 freestanding ELF32，不需要 32 位宿主 libc。宿主测试使用 ASan/UBSan；
集成测试需要 QEMU system i386 与支持 i386/Python 的 GDB。

```sh
make -j2
make profiles app-sdk apps independent-libos
make sdk-package
```

SDK 位于 build/app-sdk；压缩包位于 build/release/aelthuvor-sdk-1.0.0.tar.gz。
包含公共头、两个静态库、四个 runtime、显式启动/入口对象、链接脚本、构建规则、来源、许可证、
模板、示例源码与教程。包内没有内核、虚拟机或无关应用 ELF。下载后先验证 SHA256SUMS，再解包：

```sh
export SDK=/absolute/path/to/aelthuvor-sdk-1.0.0
cp -R "$SDK/templates/application" my-app
make -C my-app AEL_SDK="$SDK"
make -C my-app AEL_SDK="$SDK" APP_POLICY=best
```

应用实现 ael_app_main，借用不透明 Context，无需读取私有源码或重新链接内核。
可选 APP_PROFILE=preempt|coop、APP_POLICY=first|best、APP_DEVICES=0|1；默认 preempt/first/无设备。
SDK 与项目路径应不含空白。阅读[十课教程](docs/application-development/README.md)、
[API 契约](docs/sdk-reference.md)与[构建指南](docs/build-guide.md)。

## 运行与机制

先在独立 Liberums 仓库构建固定版本内核。抢占式应用运行命令：

```sh
qemu-system-i386 -machine pc -accel tcg -cpu qemu32 -m 32M -smp 1 \
  -display none -serial stdio -monitor none -nic none -no-reboot -no-shutdown \
  -kernel /path/to/Liberums/build/liberums.elf -append multi-external \
  -initrd my-app/build/app.elf
```

合作式 ELF 使用 -append external。Console 需要 APP_DEVICES=1 和可信启动授予的
multi-vga=1 multi-keyboard=1；链接设备库不授予权限。新旧 Console 保留
help/clear/about/status/memtest/exit。云端验证 QMP/PS2/IRQ1/VGA；GTK 与实体键盘仍需人工本地验证。
headless QEMU 输出完成后仍保持运行，可从另一个终端结束进程。参见[GDB 调试](docs/debugging-guide.md)。

First-fit/Best-fit 在 Page Pool 上管理对齐块、分裂与合并。单次分配 1..4096 字节；
free 保留空闲 Arena，trim 释放页并重试失败回滚记录。空闲撤销可以 ACK；活跃或丢失 Heap
采取 Fail-stop（96）；成功应用状态下的泄漏或清理错误为 95。check 不能锁住下一次访问。

[Independent LibOS](reference/independent-libos/README.md) 是另一条路线，拥有自己的启动、runtime、
syscall 和固定对象池，不链接 libael。它与 Ael 应用可在同一内核上以独立 ELF/CR3 并存。

## 验证与维护

```sh
make clean
make -j2
make profiles test sdk-tests app-sdk apps independent-libos
make package-tests
make integration LIBERUMS=../Liberums
make console-integration LIBERUMS=../Liberums
make phase4-integration LIBERUMS=../Liberums
make phase5-integration LIBERUMS=../Liberums
make phase6-integration LIBERUMS=../Liberums
```

保留 Phase 5 全部 27 个真实撤销/审计 guest；Phase 6 验证压缩包外部消费者与全部课程源码。
历史 ABI 1.1/1.2、旧 ael_main、新 ael_app_main、两种策略与 Profile、可选 Console 均保留。
六个内核公开头保留原始来源提交与 SHA256。

[验收](docs/phase6-validation.md) · [架构](docs/architecture.md) · [兼容](docs/compatibility.md) ·
[限制](docs/known-limitations.md) · [历史](docs/history.md) · [维护交接](docs/maintainer-guide.md) ·
[更新记录](CHANGELOG.md) · [发布说明](RELEASE-NOTES.md)

正式 make release-package 要求干净且已提交的工作树，产出源码/SDK、Manifest、许可证、说明和 SHA256SUMS。
归档元数据与 SDK 调试路径归一化，不承诺跨工具链 ELF 逐字节一致。生成产物不提交，原仓库由维护者接管。

这是教学/开发基线，不是生产安全或无限运行时间认证。单 CPU i686、静态 ELF32、有界资源；
没有透明指针恢复、POSIX、网络、文件系统、spawn/IPC、动态链接、SMP、线程或 GUI。
未进行小时/天级 soak 或实体键盘验证。Phase 6 交接后停止自动开发，未来架构由维护者决定。
