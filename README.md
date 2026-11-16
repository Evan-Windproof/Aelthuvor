# Aelthuvor — a user-mode LibOS for Liberums

独立的 C11/i686 Library OS，以静态 ELF32 运行于 Liberums Ring 3。GPL-3.0-or-later。
Phase 1 提供用户态页面与块分配；Phase 2 增加 ABI 1.2、抢占与撤销；Phase 3 增加 VGA/PS2 模块和独立 Console。
Aelthuvor 决定分配策略，Liberums 决定资源保护与 CPU 仲裁。两仓库独立构建，不链接内核私有对象。

## 独立构建

需要 GCC、Binutils、GNU Make、Python 3（历史源码重建使用支持 tarfile data filter 的 Python）。
Linux 宿主测试使用 memfd/mmap、ASan/UBSan。freestanding `-m32` 不需要 32 位宿主 libc。
可传 `CROSS_COMPILE=i686-elf-`；宿主测试仍使用 `HOST_CC`。

```sh
make clean
make -j2                # 默认仍是 cooperative first-fit，build/aelthuvor.elf
make test               # 四种构建 + SDK 负测试 + 两个 sanitizer 测试程序
make profiles           # first / best / preempt-first / preempt-best ELF
make abi11              # 从固定 Phase 1 Git 提交和原 SDK 重建历史 ELF
make console            # build/ael-console.elf，Preemptive first-fit
make build/console-best.elf # 可选 best-fit Console
```

| ELF（build/） | 执行方式 | Required features |
| --- | --- | --- |
| first.elf / aelthuvor.elf | cooperative first-fit | 0x1d |
| best.elf | cooperative best-fit | 0x1d |
| preempt-first.elf | preemptive first-fit | 0x75 |
| preempt-best.elf | preemptive best-fit | 0x75 |

以上新 ELF 均要求 ABI 1.2。原 Phase 1 的 ABI 1.1 映像由 `make abi11` 独立验证，
不是通过修改新 ELF 的 note 冒充旧二进制。输出 provenance 包含来源及本轮 ELF SHA256。

## 独立内核集成

仅 `sdk/include/abi/` 是受控 vendoring；来源完整提交和 SHA256 见 `sdk/manifest.json`。
构建不需要 Liberums；集成需要兼容的独立 checkout、QEMU 和支持 i386/Python 的 GDB。

```sh
make check-sdk SDK_SOURCE=../Liberums  # 更新 SDK 时核对固定来源和工作目录
python3 tests/check_sdk.py --source ../Liberums --pinned-only # 仅固定 Git 来源
make integration LIBERUMS=../Liberums # 13 guests / 26 轮生命周期
make -C ../Liberums run QEMU_APPEND='external preempt' \
  QEMU_INITRD="$(pwd)/build/preempt-first.elf"
```

集成脚本在 Aelthuvor 维护，只读取内核公开 SDK 的固定 Git 对象，运行时另做版本/特性检查。
内核 checkout 的源码不被修改；构建会生成其忽略的 build/。没有 shell；前台 QEMU 按 Ctrl+C 退出。
工具安装和本轮环境的可复现说明见 [验收记录](docs/phase2-validation.md)。

## API 与安全边界

- `ael_alloc`：16 字节对齐，每次 1..4096 字节，first-fit/best-fit 均保留分裂与合并。
- `ael_free`：仅接受精确活跃块起点；NULL 无操作；非法/重复释放 EINVAL。
- `ael_heap_trim/destroy`：归还空页；destroy 在有活跃块时 EBUSY。
- Preemptive 下公共分配器入口先检查撤销和句柄。空 Arena 可 ACK；活跃页撤销或协议错误导致
  受控 `exit(96)`。显式 `ael_heap_check` 返回 sticky 错误，调用方必须停止执行，不得忽略。
- 强制撤销可能在任意两条用户指令间发生，事件完成后消失。查询不能消除 TOCTOU，
  不保证透明恢复活跃指针；检查前访问可能由 MMU 终止本域。见 [内存安全](docs/phase2-memory-safety.md)。

真实 QEMU/GDB 已验证双 Profile、两种策略、IRQ/Peer/寄存器恢复、ABI 1.1 历史映像、隔离和清零回收。
撤销客户端已通过宿主模拟；真正的外部 Aelthuvor 撤销端到端场景尚无内核触发入口，未验证。

Console 支持 help/clear/about/status/memtest/exit、US 键盘编辑、滚动和光标。它静态链接同一域内的 LibOS，
不提供通用 Shell、多外部 ELF 并发或 IPC，也不提供 POSIX、文件系统、网络、线程、SMP、动态链接或透明指针迁移。
普通 i686 无 NX，不承诺完整 W^X。原始指针复用后不能区分旧/新对象。

阅读：[Phase 2 总览](docs/phase2-overview.md) → [架构](docs/architecture.md) →
[ABI](docs/phase2-abi.md) → [抢占](docs/phase2-preemption.md) →
[撤销](docs/phase2-revocation.md) → [模块](docs/phase2-modules.md) →
[实际验收](docs/phase2-validation.md)。[Phase 1 验收](docs/phase1-validation.md)保留为历史记录。

## Phase 3 Console

核心 SDK 仍固定在 Phase 6；两个可选设备头单独固定至 Phase 7.3，见 manifest.extensions。
INFO 探测与可信启动 grant 分开；没有新增 feature bit，默认内存程序不取得设备。

两个 checkout 同级且已有匹配的内核 ELF 时：

```sh
make console
make -C ../Liberums run-external-console \
  CONSOLE_ELF=../Aelthuvor/build/ael-console.elf
make console-integration LIBERUMS=../Liberums
```

修改 Console 只需重建 Aelthuvor，运行目标不会重新链接内核。
云端通过 QMP 注入真实 PS/2 事件，检查 IRQ1/用户态/VGA 并保存渲染截图；
本地 WSL2 GTK 实体键盘交互仍待维护者确认。普通 RAM 撤销 E2E 仍未补齐，不以设备撤销测试代替。

阅读：[Phase 3 总览](docs/phase3-overview.md)、[设备 ABI](docs/phase3-device-abi.md)、
[Console 与启动](docs/phase3-console.md)、[内存协作](docs/phase3-memory-integration.md)、
[模块](docs/phase3-modules.md)、[实际验收](docs/phase3-validation.md)。
