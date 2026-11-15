# Aelthuvor Phase 1 架构

Exokernel 提供保护机制，LibOS 提供管理策略。Liberums 只回答“这页属于哪个域实例、允许如何映射”，
不回答“应用的 96 字节申请应该落在哪个空闲块”。后一个问题由本仓库的 memory.c 决定。

```text
独立 ELF _start (arch/x86/startup.S)
    -> C ael_start：查询 ABI 与额度、初始化 page pool / heap
    -> 示例 ael_main：块申请 / 写读 / 释放 / 策略对比
    -> memory.c：查找、分裂、合并空闲块
    -> page.c：句柄元数据、选 VA、claim/map/unmap/release
    -> syscall.c：int 0x80
       ---- CPU 特权边界 ----
       Liberums syscall_dispatch -> 所有权/代数/权限 -> Secure Binding PTE
    <- iret 返回 Ring 3，普通访存直接由 MMU 检查
    -> 正常 exit 或用户异常：内核撤销该域绑定并清零回收全部私有页
```

运行时和应用位于同一个不可信用户域，所以 LibOS 的普通内存错误可能破坏应用或自身元数据。
把 LibOS 放在 Ring 0 会把分配策略和这些错误引入可信计算基，违背保护与管理分离。
内核不会因为 LibOS 出错就授予它权限；句柄不是可猜中即有效的授权。

| 模块 | 职责 |
| --- | --- |
| startup.S / runtime.c | 入口栈、ELF note、初始化、应用调用与退出 |
| compat.c / syscall.c | ABI 能力检查、标量寄存器调用、本地输出 |
| page.c | 原资源机制包装、LibOS 选择 VA、失败后保留/回滚句柄 |
| memory.c | 用户空闲块表、first-fit/best-fit、复用与合并 |
| string.c | 编译器可能生成的 freestanding memset/memcpy |
| examples/memory_demo.c | 与静态库链接的应用及真实资源检查点 |
| tests/memory_checks.c | 宿主和 guest 共用的分配行为检查 |

静态库只含本项目对象；链接脚本把入口/代码、只读内容/ABI note、数据/BSS 分为页不重叠的 LOAD。
内核按 program headers 申请独占物理页、复制文件内容并将 BSS/四页栈清零，再 iret 到 entry。
镜像本身的页不是动态资源句柄，不能由 page_release 释放。

没有中断驱动调度、SMP、用户线程、动态加载或事件撤销协议。一个不 yield 的域仍可无限占用 CPU。
