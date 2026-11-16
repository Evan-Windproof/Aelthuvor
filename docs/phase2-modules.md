# Phase 2 模块导读

| 文件 | 职责与不变量 |
| --- | --- |
| arch/x86/startup.S | freestanding 入口，清 DF、对齐栈，不选择内核策略 |
| src/runtime.c | ELF note、ABI 探测、heap 初始化、应用调用、退出 |
| src/compat.c | 版本/特性/额度检查；仅要求 REVOCATION 的实例启用事件检查 |
| src/syscall.c | 标量寄存器系统调用、log/yield/exit |
| src/page.c | 句柄、VA、映射/回滚与 terminal 状态；不写 PTE |
| src/revoke.c | POLL/INFO/ACK、QUERY、事件身份验证、bounded service、sticky heap check |
| src/memory.c | first/best-fit、分裂合并、trim/destroy；公共入口先做安全检查 |
| examples/memory_demo.c | 两种策略共用行为、真实页读写、Coop yield 或有界 CPU work |
| tests/memory_tests.c | Phase 1 宿主模拟、随机分配、失败回滚和 ABI 基线 |
| tests/revoke_tests.c | 新协议、状态、fail-stop、生命周期序列；mock 可实际 munmap |
| tests/legacy_build.py | 固定历史源码与旧 SDK 的独立重建、provenance |
| tests/guest_audit.py | Aelthuvor 自有 QEMU/GDB 审计，保留 PTE/所有权/清零断言 |
| tests/integration.py | SDK 来源检查、独立内核构建、调用自有审计 |

阅读顺序建议先看 memory/page 头文件，再看 service 和公共入口 guard。page state 的权威性有限：
内核所有权和 PTE 才是保护机制。revoke 不读 payload；失败后诊断信息保留在 BSS。

教学上应区分三种完成：编译链接完成、宿主状态机通过、真实硬件保护通过。
所有实际通过与尚未验证内容集中记录在 phase2-validation.md。
