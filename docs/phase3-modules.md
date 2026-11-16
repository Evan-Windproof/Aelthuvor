# 模块与维护入口

| 模块 | 职责 |
| --- | --- |
| runtime/syscall/page/memory/revoke | 原有 ABI 探测、调用、RAM 生命周期、分配策略与安全失败 |
| include/aelthuvor/vga.h、src/vga.c | 设备句柄、权限、映射、失效、边界受限的单元/fill/scroll |
| include/aelthuvor/keyboard.h、src/keyboard.c | 探测、acquire/poll/stats/release、纯 set-1 解码器 |
| include/aelthuvor/text.h、src/text.c | row/column、属性、清屏、换行、滚动、Backspace、数字输出 |
| apps/console/console.c | 应用初始化与回滚、固定命令、Heap 缓冲区、输入编辑 |
| apps/console/main.c | ael_main、持续循环、观察点、正常退出 |
| apps/console/device_probe.c | 独立的公开 ABI 负测试 ELF，不用于生产 Console |
| tests/device_tests.c | 实际设备模块 + 实际分配器 + syscall mock，ASan/UBSan |
| tests/console_integration.py | Aelthuvor ELF 的 QMP/PS2/GDB/显存验收及来源 SHA |

VGA、Keyboard 对象必须零初始化后 acquire，不要重新初始化一个仍持有句柄的对象。
`ael_console_open` 也只用于新对象，close 后不再访问原 line 指针。没有同域线程安全承诺。
成功系统调用才提交本地 mapped/owned 状态；失败保留可清理记录或标为失效。
text.error 是 sticky：绘制失败后后续调用不再访问显存，应用停止运行。

Console 的设备对象和布局状态在应用结构中，设备模块不解释命令；文本模块不分配普通资源；
输入解码器不访问硬件；内核只看到标量 syscall，不理解 help/memtest。
旧程序继续只链接 libaelthuvor.a；设备 .o 单独选择加入 Console 的链接命令。

后续 Phase 4 可以在已有 ael_* 包装之上明确应用上下文、统一错误传递和可替换输出接口，
减少应用直接读取 ael_runtime 的需要，再为其他静态应用复用。当前不提前实现动态链接、IPC 或多进程启动器。
