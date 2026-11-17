# LibOS 兼容矩阵

以 Phase 4 实测及固定 Liberums Phase 8 内核为准；“可构建”“可装载”“运行并回收”是不同结论。

| 程序 | 来源/接口 | Profile / required | 实际结果 |
| --- | --- | --- | --- |
| Phase 1 原始源码/SDK ELF | 固定 b10d337 历史重建，ABI 1.1 | Cooperative / 0x1d | 旧 suite 真实运行及双轮回收 |
| Phase 2 内存示例 | 旧 ael_main，ABI 1.2 | Cooperative 0x1d / Preemptive 0x75 | 两策略、IRQ/恢复、隔离/错误/回收 |
| Phase 3 Console | 旧应用结构/设备 wrapper | Preemptive / 0x75 | 原 9 guest 套件保留，通过 |
| SDK Hello / Memory | 新 Application API 1.0 | Preemptive / 0x75 | 单独运行、两策略及退出回收 |
| SDK Hello coop | 同一应用 API | Cooperative / 0x1d | external 双轮；multi profile 正确拒绝 |
| SDK Console | 新公开上下文/可选服务 | Preemptive / 0x75 | QMP PS2、命令、Heap、IRQ、退出 |
| 外部应用模板 | 只使用导出 SDK | Preemptive / 0x75 | 隔离改源码/构建后真实运行 |
| Independent Reference | 自有 runtime/pool，内核 ABI 1.2 | Preemptive / 0x35 | 隔离构建、单独和与 Aelthuvor 并存运行 |

0x75 与 0x35 是程序所需特性，不是 LibOS 品牌识别。参考没有 ACK 客户端，因此不声称实现了撤销响应策略。
新旧 Kernel ABI、Application API 版本分别记录，未升级任何内核公开版本或来源。

多域实测覆盖两个独立实现，不宣称用户态 spawn/IPC 或同址共享。内核既有最大四域不变。
完整 guest/生命周期计数、ELF SHA 和未执行项见 phase4-validation.md。
