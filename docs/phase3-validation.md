# Phase 3 实际验收

本记录来自 2026-11-16 当前云环境实际执行。Aelthuvor 起点和开发前远程 main 为
`1d288be418ddeef89462914769d5ea84bb557ade`。Liberums 远程 main 与只读参考 checkout
均固定为 `5d9c8f35a4ae54ef08b93b7f4d5fecd9e350acbf`。未修改、提交或推送内核源码。

## 三类证据

1. **宿主模拟：通过。** 使用实际设备模块、文本模块、Console 与真实分配器，syscall 由 mock 提供。
2. **QEMU/GDB 硬件模拟：通过。** 由实际 Aelthuvor ELF 执行；输入经 QMP → PS/2 → IRQ1，读取真实 VGA 显存和渲染截图。
3. **WSL2 GTK 实体键盘：未执行。** 由维护者后续亲自确认，云端 QMP 成功不能替代该项。

## 构建与回归

本轮复用已有 GCC 14.2.0、Binutils 2.44、Make 4.4.1、Python 3.12.14，
仓库外 QEMU 10.0.13、GDB 16.3（带 Python/i386）。没有重新修复或发布云环境。
参考内核在 `/tmp/aelthuvor-liberums-reference`，工具包装器在 `/workspace/aelthuvor-tools/bin`。
标准系统可直接使用已安装的 QEMU/GDB，来源安装说明沿用 phase2-validation.md。

```sh
make clean
make -j2
make profiles
make test
make console-integration LIBERUMS=/tmp/aelthuvor-liberums-reference \
  QEMU=/workspace/aelthuvor-tools/bin/qemu-system-i386 \
  GDB=/workspace/aelthuvor-tools/bin/gdb
make integration LIBERUMS=/tmp/aelthuvor-liberums-reference \
  QEMU=/workspace/aelthuvor-tools/bin/qemu-system-i386 \
  GDB=/workspace/aelthuvor-tools/bin/gdb
python3 tests/check_sdk.py --source /tmp/aelthuvor-liberums-reference
git diff --check
```

| 检查 | 实际结果 |
| --- | --- |
| 默认 make、profiles、console | 通过；ELF32 静态链接、无未解析符号 |
| SDK 核心/扩展 SHA256 与固定 Git 来源 | 六个头均通过；四个核心头字节及来源保持 Phase 2 不变 |
| SDK 漂移负测试 | 10 项通过，包含核心/扩展缺失、篡改、重复来源文件、非精确提交 |
| Phase 1 ASan/UBSan | 77,438,415 条断言通过 |
| Phase 2 撤销/Profile ASan/UBSan | 7,789 条断言及 1,000 轮生命周期序列通过 |
| Phase 3 设备/解码/文本/应用 ASan/UBSan | 853 条断言通过；另处理 10,000 字节解码序列，无 sanitizer 报告 |
| 原有 integration | 13 guests / 26 轮生命周期，无跳过；含 ABI 1.1 历史源码/SDK ELF、两种策略与双 Profile |
| 新 console-integration | 9 guests；11 轮完整生命周期 + 1 个持续交互 guest，通过 |
| 实际 run-external-console 目标 | 另一个启动 smoke guest：GDB 确认 Aelthuvor 欢迎文字、CPL3、Heap；检查后停止，不计完整生命周期 |

宿主覆盖无接口 ENOSYS、无 grant EPERM、错误扫描格式、map 权限/地址、颜色和边界、换行/滚动/光标、
旧 generation、设备失效后禁止写入、键盘 EAGAIN/EOVERFLOW/ESTALE、释放与重复释放。
用 VGA 页最后 96 字节中的 canary 检查绘制不越界；失效场景将映射设为 PROT_NONE，检验包装器不会继续触碰它。
编辑覆盖 Shift/Caps/双 Shift、Backspace/Enter、E0/Pause、溢出后状态重置与 63 字符上限。
两个分配策略都实际运行应用命令；测试分配不足、部分初始化回滚、free/trim 和退出后 live/owned 恢复。
这部分模拟结果不冒充内核 Capability 或 IRQ 保护验证。

## 真实 Aelthuvor Console

新套件只导入内核测试的 QMP/GDB 传输与硬件辅助函数，不调用内核 Console 实现替代 Aelthuvor。
QEMU 的 initrd 明确指向 build/ael-console.elf 或对应 best/test/probe 变体。
生产映像、best-fit 映像和诊断映像均来自本仓库源码、启动入口、runtime、分配器和受控公开 SDK。
Console note 实测为 (major=1, minor=2, required=0x75)。

| Guest | 验收 |
| --- | --- |
| 持续 first-fit Console | 完整输入/命令/滚动；捕获 PNG 后由测试停止 VM，不计作正常退出 |
| first-fit exit | 实际输入 exit，清理并正常结束 |
| best-fit exit | 相同交互和 memtest，检查实际 policy=best-fit |
| console-test #UD | 私有诊断分支触发真实 vector 6，内核清理设备/RAM |
| VGA 强制撤销 | 既有可信诊断入口 revoke VGA；观察 ESTALE、本地 cells 失效、PTE 删除，键盘仍有效，再退出清理 |
| Keyboard 强制撤销 | 队列/能力失效，VGA 映射仍有效；应用停止、释放其余资源 |
| 无 grant | 同一 Console 在 external preempt 被拒绝 acquire；两轮安全退出与清理 |
| 只读 VGA grant | RW acquire 被拒绝，无越权；两轮清理 |
| device-probe | 真实公开 ABI 非法 VA、设备句柄传 RAM QUERY、旧 VGA/KBD generation、重复释放与末单元写入；两轮通过 |

交互 guest 实际注入 help/about/status/clear/memtest、无效命令、Shift/Caps、Backspace、方向键、超长行和滚动。
GDB 检查 CPL3/SS、私有栈、独立 CR3、IF/IOPL、IRQ0/EOI、抢占后继续处理输入、IRQ1 原始现场/IRET 恢复、
内核键盘队列字节到用户 POLL 的顺序、设备 owner epoch/generation 和 VGA PTE/UC 属性。
硬断点按内核 IRQ 序号去除调试器重复停顿，不把重复停顿计作新的硬件 IRQ。

VGA PA 为 0xb8000，映射在 0x09400000；输入行在 RAM Window 中的真实用户 RW 页，
其 PFN 与 VGA 不同。属性字节为 0x1f。memtest 实际申请/写读/free/trim，成功后 live=1，
只有输入缓冲区留下；正常关闭后 RAM/设备页表、grant、队列均清理并恢复 free count。
没有向内核队列或显存写测试字符串来伪造结果。强制撤销只通过内核已有的显式
`selftest=device-revoke` 诊断控制变量请求，不添加 grant，不修改内核源码。

## 映像来源与内核不重链证据

集成脚本先构建匹配基准的内核，然后记录其 SHA256 和 mtime；随后用 `make -B` 实际重建
Aelthuvor Console/变体并运行，最后核对内核 SHA 和 mtime 均未改变。
内核第一次准备需要构建；后续修改 Console 不需要重新链接内核。
另实际调用 run-external-console，指定本仓库产物及 QEMU_DISPLAY=none，用 GDB 停在 Aelthuvor 观察点，
确认欢迎文字、CPL3 和 Heap。该目标虽会准备 Liberums 自带外部用户 ELF，但没有替换指定 initrd，
内核 SHA/mtime 仍不变。

本轮 ELF 哈希（包含调试信息，随路径/工具链变化可能不同）：

- build/ael-console.elf：`a74d5c9d7e535082089572a20e9cc928e165a6d7da2ccac922dea866ef2d88e5`
- build/console-best.elf：`ca5456b174c01618cfe16ed0ba7de7f688094aee5139343ced7283254a010be2`
- 参考内核 build/liberums.elf：`a9d4df341e12cd0810cd1a486c0b83d8da55c8f90c78e30b85ded83115ff499b`

机器可读记录在忽略的 build/phase3-results.json；QEMU 渲染图在 build/ael-phase3-*.png，
GDB/串口日志在参考内核的忽略 build/ 下，均不提交为源码。
[本轮持续 Console 截图](../build/ael-phase3-persistent.png)随本机 build/ 保留，clean 后需重新验收生成。

## 剩余边界

尚未执行本地 GTK 实体键盘交互；请按 phase3-console.md 的 WSL2 命令启动，点击 QEMU 窗口，
检查大小写、Shift/Caps、编辑、滚动、五个必需命令与 exit。

设备撤销和普通 RAM Visible Revocation 不同。本轮没有外部普通活跃页面撤销的完整触发实验，
不得用上述两种设备 revoke 替代该验收。查询不能消除强制失效与下一次访问之间的竞态；
当前只提供发现失效后的停止和内核故障隔离，不提供 pin、用户异常恢复或透明指针迁移。

仍只有一个外部静态应用，不宣称多个外部 ELF、IPC、POSIX 或通用 Shell。到此停止，不进入 Phase 4。
