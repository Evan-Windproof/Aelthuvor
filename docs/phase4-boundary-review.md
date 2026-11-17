# Phase 4 边界审查

| 决策 | 边界 |
| --- | --- |
| 不透明 Application Context | 隐藏普通应用不需要的内部结构；不提供新的权限或同域内存保护 |
| app-entry 显式对象 | 新/旧入口可以分别演进，不把具体 main 放入静态库 |
| 独立设备 archive | core 没有设备强依赖，打开设备也只能消费已授予能力 |
| 新 SDK Console | 命令留在应用；低层 vga/keyboard/text 继续复用，未把解释器放回内核 |
| 固定对象池参考 LibOS | 独立源码、启动、链接和策略；不继承 Aelthuvor 内部实现 |
| Phase 8 并存 | 使用真实 Multiboot 多模块和可信启动配置，没有用户 spawn 或一个 LibOS 启动另一个 |

普通应用只用 app.h / app_console.h。低层头仍可用，但直接改 heap、绕过包装调用 syscall、
伪造本地 device 标志会破坏本域记账，不影响内核独立的所有权/epoch/generation/rights 检查。
同域 C 代码不是沙盒；opaque context 的意义是模块边界和维护，不是防恶意代码。

recoverable 错误用负数传播；失效活跃 RAM 由 fail-stop 终止，不返回旧 Arena 的指针。
设备失效停止 Console service 并清理其他设备；VGA 不进入 RAM 归还路径。
检查不能 pin 页面，强制撤销仍可能在下一次普通访问触发 #PF。独立参考更保守：QUERY 失败即结束，
没有 Visible Revocation ACK 策略，也不宣称透明恢复。

诊断参考 ELF 才接收私有对抗输入，验证真实 EPERM/ESTALE/#PF。GDB 不修改页表、资源所有者或 syscall 成功结果。
两 LibOS 正常并存使用生产参考 ELF；错误试验不冒充生产的 IPC 接口。

仅修改 Aelthuvor；未增加内核 syscall、feature bit、权限或 ABI 版本。
没有 GUI、POSIX、文件系统、网络、动态链接、SMP、线程、fork、spawn 或通用 IPC。
Phase 8 并存上限仍为四域、单 CPU，每域动态 RAM 四页，设备仍独占。
