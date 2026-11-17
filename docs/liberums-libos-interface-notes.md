# 为 Liberums 编写 LibOS：中立接口笔记

这是供未来 Liberums Phase 10 参考的实验初稿，不是最终官方教程。
Aelthuvor 的 ael_* 是一种 LibOS 的接口，不是内核 ABI。开发另一种 LibOS 不需要 Aelthuvor runtime 或其目录结构。

## 公开组成与版本

include/abi/syscall.h、resource.h、elf.h、event.h 定义 ABI 版本、标量调用、资源、ELF note 和撤销。
可选 vga.h/keyboard.h 描述设备接口。受控副本的固定提交与 SHA256 在 sdk/manifest.json；
本轮核心 ABI 1.2，minor 新版兼容需要 major 相同、minor 不旧、required features 全存在。
可选设备没有单独 feature bit，必须 INFO 探测，再由 ACQUIRE 验证 grant。

## ELF 与入口

内核校验 ELF32 little-endian i386、静态 ET_EXEC、PT_LOAD 权限、范围、文件/BSS 和 PT_NOTE。
参考源码在 reference/independent-libos/link.ld、start.S、runtime.c。它把代码、只读内容、可写/BSS 按页分开，
入口清 DF、对齐栈后调用自己的 C runtime；不调用任何 Aelthuvor 函数。
用户栈由 loader 私有创建在 0x083fc000..0x08400000。相同用户 VA 在不同 CR3 下并不共享物理页。
普通 i686 无 NX，不能将加载器的 RX/R/RW 约束描述为完整硬件 W^X。

```c
/* 本参考程序实际使用 PAGES | ELF32 | LIMITS | PREEMPTION，即 0x35。
 * 它不实现撤销 ACK，不能要求/宣称自己的可见撤销客户端已经存在。 */
int32_t version = reference_call(SYS_ABI_QUERY, 0, 0, 0, 0, &features);
```

ELF note 和运行时检查都要做。不兼容加载通常 -93，不能将 loader 拒绝与已启动程序主动退出混为一谈。
Cooperative 不保证强制 CPU 隔离；Phase 8 multi-external 是 Preemptive，要求合适的 feature 集。
抢占会保存/恢复同域指令流，不等于多线程；本参考用有界 CPU work、普通栈和无 FPU 编译选项真实验证恢复。

## 系统调用与资源

int 0x80：EAX 号/状态，EBX/ECX/EDX/ESI 标量参数。指定 EDX 为额外输出，其余寄存器按公开契约保持。
错误读 EAX 的负数；Generation、Request ID 等 EDX 数据必须保留完整 uint32_t。
extra 指针只由用户 wrapper 写入，内核不解引用它。可读参考目录 syscall.c 的独立汇编包装。

一个 RAM handle 是 index+generation，不能把 PFN 当权限。CLAIM 授予资源，MAP 请求 VA/rights，
内核验证 owner ID/epoch、完整 generation 与权限，再写受保护 PTE。普通访问由 MMU 检查。
查询 quota 不代表保证有空闲物理页；ENOMEM、ENOSPC 和失败清理必须处理。
RELEASE 撤销映射并清零归还；若失败，LibOS 不应丢掉还需要清理的记录。

内核保护“谁可使用资源”；LibOS 选择 VA、固定池/线性 Arena/first-fit 等策略；应用消费这个 LibOS 的接口。
这些策略都不是内核强制的规则。本参考仅一页 64 个 64B 对象，最高空槽优先、LIFO 复用；
可以按教学目标自行换成不同策略，不需要继承 Aelthuvor。

## 撤销、设备与多域

Visible Revocation POLL/INFO/ACK 只通知活动请求，完成后事件可能消失。ACK 表示不再依赖资源，
不是继续使用内存的许可。没有通用 pin/完成 tombstone/用户异常恢复时，不承诺透明恢复任意活跃指针。
本参考未实现客户端；QUERY 检查失败会停止，之后的竞态仍可能由 MMU #PF 终止本域。

VGA/Keyboard 使用独立 namespace、generation、grant 和生命周期，不是普通 RAM 页。
字符布局、扫描码解释、命令属于用户态；IRQ/PIC/Capability/PTE 属于内核。INFO 可发现设备不代表有权限。
设备 grant 来自可信 boot 配置；ELF 名称、品牌或 note 不授予权限。

Phase 8 最多四个并存域、八个 boot 模块，启动者逐个验证；单 CPU，各域不同 CR3、栈与资源配额。
同一次 QEMU 可用逗号 initrd 列表真正装入两个 ELF：见 independent-libos-reference.md。
内核并未提供用户 spawn/fork、通用 IPC、POSIX、文件系统、网络、动态链接或设备复用服务。

构建、GDB 与错误对抗的实际证据见 phase4-validation.md、兼容矩阵；本地 GTK 人工交互与云端 QMP 不等价。
