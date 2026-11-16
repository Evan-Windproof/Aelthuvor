# 抢占与恢复

`make profiles` 构建 first/best/preempt-first/preempt-best 四个 ELF。默认 make 不变，仍选 first。
Preemptive 运行时探测要求 0x75；pool.revocation 由实际 required feature 决定，
cooperative 不调用 24..26。内核必须以 `external preempt` 启动；profile 不匹配由 loader 拒绝。

入口仍清 DF、对齐私有栈、调用 ael_start。通用寄存器参数、EAX/EDX 输出和 memory/cc clobber
沿用公开 SDK。所有对象使用 -msoft-float/-mno-mmx/-mno-sse/-mno-sse2，不依赖跨域 FPU 状态。

内核保存/恢复异步 IRQ 的完整用户现场。暂停可以发生在块表修改中途，但恢复后仍由相同域、
相同调用栈继续执行；没有同域线程或异步回调去观察半完成的元数据。因此无需复杂锁，
撤销服务只在公共入口及显式安全点执行，不在分裂/合并中递归处理事件。

普通抢占不等于资源失效。若同时发生资源强制撤销，元数据恢复不代表原地址仍映射；
安全点检查和失败策略见 phase2-memory-safety.md。

Preemptive demo 保留原始分配行为测试，写入 1024 个字后执行有界 12,000,000 次 CPU 循环，
循环不 yield、不调用系统调用；随后显式检查，核对数据，再释放页面。
`ael_cpu_begin/end` 是真实循环的汇编标签，不决定测试成功。

真实 auditor 从 launch 起观察 peer/checkpoint/IRQ，而不是假设 Peer 必须等到某个应用检查点后才运行。
每轮分别要求：独立 CPL3/CR3、IRQ vector=32、IF、TSS 栈、CPU 循环内 EIP、无主动 yield、
Peer 获得 CPU（当时 A 的 preemptions>=1）、精确 GP/ESP/SS 恢复、算术标志/DF 与 IF 恢复、
后续数据检查成功和最终清零回收。硬件断点最多四个，不用软件断点污染跨 CR3 共享地址。
