# Aelthuvor 架构（Phase 1～3）

```text
独立 ELF _start -> ael_start -> ABI/额度探测 -> heap -> ael_main
    allocator 公共入口 -> 同步 revoke service（Preemptive）
    memory.c 策略 / 块表 -> page.c 句柄与 VA -> syscall.c int 0x80
       ---- Ring 3 / Ring 0 ----
    Liberums：资源所有权、epoch/generation/rights、PTE、IRQ 时间片与最终回收
```

LibOS 和应用在同一个不可信域；元数据位于 image/BSS。内核不信任块表，不允许 LibOS 写 PTE。
普通访存由 MMU 执行；分配策略在用户态，静态库不包含内核对象。
链接脚本按页分开代码、只读 ABI note、数据/BSS，私有栈由 loader 创建。

单 CPU、每域单线程：IRQ 暂停/恢复不导致同域并发写入。事件不异步 upcall，只在入口/检查点处理。
两个 Profile 的主要区别是 note、runtime required features、pool 检查开关和示例 CPU 行为。

保护与可用性必须区分：强制撤销仍可能使任意普通指针访问 #PF。LibOS 对已观察到的活跃撤销选择
失败停止，既不更改内核保护，也不假称透明修复指针。详见 phase2-memory-safety.md。

完整模块表见 phase2-modules.md；Phase 1 原始架构可从基准提交查看。

## Phase 3：可选设备与静态应用

新增 apps/console 应用，通过独立的 vga/keyboard/text 模块实现命令、编辑和绘制。
设备对象不进入 RAM pool，Console 的输入缓冲区使用原分配器。设备模块单独参与应用链接，
不增加默认内存示例的执行权限或运行依赖。两个设备头在 SDK 中单独固定来源。

可信 external-console 配置授予设备并开启 IRQ0/IRQ1；用户态 INFO 只探测，ACQUIRE 才检查授权。
仍只有一个外部应用域，LibOS 与应用静态组合；Peer 测试不代表多个外部 ELF/IPC 能力。
完整分层和未来维护入口见 [Phase 3 模块](phase3-modules.md)。
