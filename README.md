# Aelthuvor — an independent Library OS for Liberums

独立 C11/i686 LibOS，静态 ELF32，运行于真实 Ring 3。GPL-3.0-or-later。
Liberums 保护物理资源、Capability、PTE 和 CPU；Aelthuvor 选择内存和用户态服务策略。
Aelthuvor 不是内核的必需运行时，也不是唯一获支持的 LibOS。

Phase 1～3 保留可变块分配、ABI 1.2、抢占/撤销客户端和设备 Console。
Phase 4 新增 **Application SDK 1.0** 和 **不依赖 Aelthuvor 的独立 LibOS 参考**。
Phase 5 完成失败回滚资源的 Trim/Finish 重试，以及 Liberums v1.0.0 上的真实 RAM 撤销生命周期验收。
两条路线分别说明，不能把 ael_* 当成内核官方 ABI。

## 使用 Aelthuvor 开发应用

```sh
make app-sdk               # 导出 build/app-sdk
make apps                  # Hello、Memory 两策略、SDK Console、独立应用模板
make app-hello
make app-memory
make app-console           # 新公共 API Console，build/apps/console/app.elf
make sdk-tests             # 隔离构建、改源码、切换策略、依赖与入口审计
```

应用只包含 app.h / 可选 app_console.h，实现 `int ael_app_main(struct ael_app_context *ctx)`。
接口提供 ABI/API 信息快照、分配/释放、错误、log/yield/exit 和可选设备服务，不暴露 Heap/Handle/Generation。
源码与外部 Makefile 示例见 [application-template](examples/application-template/README.md)。
复制导出的 SDK 和模板到仓库外即可编译，无需 src/ 或内核 checkout。
修改应用只重建对应应用，不重编译 LibOS 或内核。默认 Preemptive，策略与可选设备通过 app.mk 参数选择。

## 直接开发另一种 LibOS

```sh
make independent-libos    # build/independent-libos/reference.elf
```

[独立参考](reference/independent-libos/README.md)有自己的 Assembly/C 入口、linker、syscall、固定对象池和应用。
它只用公开 ABI 头，不包含 aelthuvor 头，不链接 Aelthuvor 的 runtime 或库。
不同策略不需要继承 Aelthuvor 的结构；这份参考是未来 Liberums Phase 10 教学的实验初稿。

在已构建的 Phase 8 内核上，可在同一次启动中加载两个 ELF：

```sh
qemu-system-i386 -machine pc -accel tcg -cpu qemu32 -m 32M -smp 1 \
  -display none -serial stdio -monitor none -nic none -no-reboot -no-shutdown \
  -kernel ../Liberums/build/liberums.elf -append multi-external \
  -initrd build/apps/memory/app.elf,build/independent-libos/reference.elf
```

两个域有独立 CR3、栈和资源，不要求其中一方启动另一方。可信启动最多四域；没有用户 spawn 或通用 IPC。

## 旧入口与独立回归

需要 GCC、Binutils、GNU Make、Python；freestanding -m32 不需要 32 位宿主 libc。
Linux ASan/UBSan 测试使用 memfd/mmap；历史重建需要支持 tarfile data filter 的 Python。

```sh
make clean
make -j2                  # 默认仍为旧 cooperative first-fit，build/aelthuvor.elf
make profiles             # first / best / preempt-first / preempt-best
make console              # 旧 Phase 3 兼容 Console，build/ael-console.elf
make test                 # 原有断言 + SDK/设备/Application/参考池宿主测试
make integration LIBERUMS=../Liberums
make console-integration LIBERUMS=../Liberums
make phase4-integration LIBERUMS=../Liberums
make phase5-integration LIBERUMS=../Liberums  # 固定 v1.0.0 checkout，QEMU/GDB
```

原 `ael_main(heap)` 不删除，与新入口通过显式对象分别选择。两个静态库都不含具体应用 main。
`make abi11` 从固定 Phase 1 源码和旧 SDK 重建原映像，保留真实历史兼容验证。
SDK 核心仍固定 Phase 6、设备扩展仍固定 Phase 7.3；六个头的来源、文件集合与 SHA256 门禁不变。
**Kernel ABI 1.2 != Aelthuvor Application API 1.0**。

新/旧 Console 均支持 help/clear/about/status/memtest/exit、US 键盘编辑、光标和滚动。
可用既有 `make -C ../Liberums run-external-console CONSOLE_ELF=/absolute/path/to/app.elf`，
修改 Console 不需要重链内核。设备由可信启动授予，INFO 发现不等于 ACQUIRE 获权。
云端使用 QMP/PS2/GDB/显存，WSL2 GTK 实体键盘仍待人工验证。

## 安全与范围

First-fit/best-fit、16 字节对齐、块分裂/合并、回滚、trim/destroy 继续保留。
严重 Heap 失效采取 exit(96)，不返回已知失效指针；设备错误停止服务并清理其余设备。
查询无法消除强制撤销后的访问竞态，不保证透明恢复任意活跃页。
Phase 5 已验证外部 RAM 的主动 ACK、真实 PIT 强制回收、exit(96)、预期 #PF 和多域隔离。

没有 POSIX、文件系统、网络、动态链接、SMP、线程、fork、通用 spawn/IPC、设备复用或 GUI。
普通 i686 无 NX，不承诺完整 W^X。旧裸指针在地址复用后不能区分新旧对象。

阅读：[Phase 4 总览](docs/phase4-overview.md) → [应用 API](docs/phase4-application-api.md) →
[独立应用](docs/phase4-independent-apps.md) → [模块](docs/phase4-modules.md) →
[中立内核接口](docs/liberums-libos-interface-notes.md) → [独立 LibOS](docs/independent-libos-reference.md) →
[兼容矩阵](docs/libos-compatibility-matrix.md) → [移植观察](docs/libos-porting-observations.md) →
[验收](docs/phase4-validation.md) / [边界](docs/phase4-boundary-review.md)。
Phase 1～3 的实现与验收文档保留为历史记录。

当前可靠性说明：[Phase 5 总览](docs/phase5-overview.md) →
[资源生命周期](docs/phase5-resource-lifecycle.md) → [撤销](docs/phase5-revocation.md) →
[失败语义](docs/phase5-failure-handling.md) → [验证](docs/phase5-validation.md) →
[边界审查](docs/phase5-boundary-review.md)。Phase 1～4 验收文档保留各阶段的历史结论。
