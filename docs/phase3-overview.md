# Phase 3：Console 与可选设备 ABI

Aelthuvor 是独立的 Library OS，用来验证公开 Exokernel 接口能否支持用户态资源管理。
它不是 Liberums 的必需组件，也不把内核限制为只支持一种 LibOS。Liberums 的实验经验还可用于未来的 Orfreon。

本阶段在 Phase 2 `1d288be418ddeef89462914769d5ea84bb557ade` 之上添加设备模块和一个静态链接应用。
内核基准是 Liberums `5d9c8f35a4ae54ef08b93b7f4d5fecd9e350acbf`，内核源码未修改。

内核负责能力、设备 grant、页表、IRQ0/IRQ1 和隔离；Aelthuvor 负责扫描码解释、绘制、编辑、命令和内存策略。
用户态不做端口 I/O，不写 PTE，不实现 IRQ 或 PIC EOI。

应用使用现有 runtime、真实 ael_alloc/ael_free、VGA 和 Keyboard 包装；旧内存示例继续使用原来的静态库。
默认 make 行为不变。Console 是 Preemptive，要求 ABI 1.2 + 0x75，设备支持另由 INFO 探测，授权另由 ACQUIRE 验证。

本阶段只有一个外部 ELF，LibOS 和应用在同一域内静态链接。内核测试 Peer 不代表支持多个独立外部应用、
IPC 或通用进程管理。Console 有固定命令表，不是通用 Shell。

阅读顺序：[设备 ABI](phase3-device-abi.md) → [Console](phase3-console.md) →
[内存协作](phase3-memory-integration.md) → [模块](phase3-modules.md) → [实际验收](phase3-validation.md)。
