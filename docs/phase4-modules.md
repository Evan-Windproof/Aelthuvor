# 模块与公共边界

| 层 | 文件/产物 | 职责 |
| --- | --- | --- |
| Kernel ABI | sdk/include/abi | 受控的 Liberums 公开头，来源保持 Phase 6 / 7.3 |
| 低层 LibOS | page/memory/revoke/syscall、libaelthuvor.a | 原有 RAM 策略、句柄包装、错误与撤销 |
| Application API | include/aelthuvor/app.h、src/app.c | 不透明上下文、快照、alloc/free、错误、log/yield/exit |
| 可选服务 | app_console.h、app_console.c、libaeldevices.a | 组合原有 VGA/Keyboard/text，提供受检查的行输入和绘制 |
| 启动 | startup.o、runtime-*.o | i686 入口、ELF note、ABI 探测、选择分配策略 |
| 新入口适配 | app-entry.o | 实现 ael_main，绑定上下文、调用 ael_app_main、统一终结 |
| 普通应用 | apps/hello、memory、sdk-console | 只使用公共应用头和 ael_app_*；没有私有全局依赖 |
| 外部消费者 | examples/application-template | 独立 Makefile，使用导出的 SDK 文件 |
| 另一 LibOS | reference/independent-libos | 自己的启动、链接脚本、syscall、pool 和应用 |

libaelthuvor.a / libaeldevices.a 都不包含具体应用、_start、ael_start、ael_main 或 ael_app_main。
运行时与入口是显式对象；应用只能选择旧入口或新 adapter，不能同时链接两个 ael_main。
静态链接器会拒绝冲突，构建测试实际验证这一点。

app.h/app_console.h 是新的推荐应用接口。memory/page/vga/keyboard/text 头继续供高级低层调用，
它们的结构体是 LibOS 状态，不是内核 ABI 布局。初始化持有资源的对象仍然非法，不能混用同一资源的两套记账。
ael_runtime 全局、src/app_internal.h、观察点和测试私有控制量不属于新应用接口。

src/app_internal.h 不导出；上下文存储和 service cleanup hook 只在库内部使用。
导出器仅复制公开头、内核 SDK、已构建对象/静态库、链接脚本、许可证和 app.mk，没有复制私有源码。
设备功能不被 core 反向依赖；Hello/Memory ELF 的符号检查证明未拉入 VGA/Keyboard/text。
