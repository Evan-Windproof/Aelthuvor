# Aelthuvor 架构（Phase 2）

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
