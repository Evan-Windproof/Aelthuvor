# Phase 4：Application SDK 与独立 LibOS

本阶段基于 Aelthuvor `46c7284325cdb04f7f6fd6a96377cf8ab9721a67`，内核参考固定至
Liberums Phase 8 `449c78f9b12535cded2bca575cd44e0b65a0953b`。两端开发前远程 main 均与指定基准相符。
不修改内核源码，不重新实现分配器、设备包装或撤销状态机。

这里有两条不同的开发路线：

- **使用 Aelthuvor 开发应用**：链接它的 runtime、入口适配对象和静态库，实现 ael_app_main。
- **为 Liberums 开发另一种 LibOS**：只使用内核公开 ABI，自行实现启动、策略与应用接口。

Aelthuvor 不是 Liberums 必需组件、官方唯一 LibOS 或特权应用。内核不识别品牌、文件名或 ael_* 符号。
独立参考采用固定对象池，Aelthuvor 继续使用 first-fit/best-fit；两者在同一次真实多模块启动中运行。

应用 API 1.0 与内核 ABI 1.2 独立演进。SDK 核心和设备扩展的来源及 SHA256 完全保留。
旧 ael_main(heap)、默认 make、profiles、Phase 3 Console 都保留；新 SDK Console 是独立、功能兼容的应用构建。

导读：[应用 API](phase4-application-api.md)、[独立应用](phase4-independent-apps.md)、
[模块](phase4-modules.md)、[独立 LibOS](independent-libos-reference.md)、[真实验收](phase4-validation.md)。
