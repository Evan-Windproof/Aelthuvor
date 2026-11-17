# Aelthuvor Application API 1.0

`AEL_APP_API_VERSION=0x00010000` 是 LibOS 自己的 C 接口版本，**不是** `SYS_ABI_QUERY` 的返回值。
当前内核 ABI 为 1.2（0x00010002）；接口错误值沿用负数约定，但以 AEL_APP_* 命名供普通应用使用。
未来两者分别演进，不能把 API 版本写入内核 ELF note。

```c
#include <aelthuvor/app.h>
int ael_app_main(struct ael_app_context *ctx)
{
    struct ael_app_info info;
    if (ael_app_get_info(ctx, &info)) return 1;
    unsigned char *p = ael_app_alloc(ctx, 80);
    if (!p) return 2; /* ael_app_error(ctx) 保留具体原因 */
    for (unsigned i = 0; i < 80; ++i) p[i] = (unsigned char)i;
    if (ael_app_free(ctx, p) || ael_app_trim(ctx)) return 3;
    return ael_app_log(ctx, "Application finished\n") ? 4 : 0;
}
```

上下文由 SDK 在 callback 前绑定，应用只借用到 main 返回或 app_exit，不申请、不复制、不释放它。
没有公开的 heap、Handle、Arena 或 Generation 字段。它不构成同域内存安全边界，也不授予内核权限。

| 函数 | 语义 |
| --- | --- |
| get_info | 拷贝 API/kernel 版本、features、策略、额度、owned/live 快照，不提供可变内部结构 |
| alloc/free/trim | 使用原分配器；16 字节对齐、1..4096 字节；NULL、非法 free 和配额错误保持原语义 |
| check | 同步 RAM 检查；发现 sticky failure 或活跃页失效直接 exit(96) |
| error | 返回最近操作状态，不清除；常规成功操作置零 |
| log | 输出到有额度的内核诊断字符通道，返回真实 EAGAIN 等错误，不假装完整输出 |
| yield | 显式 CPU 让出；Preemptive 程序不依赖它获得强制 CPU 仲裁 |
| exit | 终结服务/heap 并 syscall EXIT，永不返回 |

成功 main 返回会先关闭可选服务、heap_destroy；有活跃分配时不伪装释放，成功状态改为 95，
即使 live=0，失败 map/rollback 仍可能留下 claim，终结也检查剩余 owned 并报告失败。
由内核最终回收。初始化失败沿用 93/94；unsafe heap 直接 96。应用本身非零状态优先保留；
这不是 POSIX wait status，内核保存完整 32 位退出值。

## 可选 Console 服务

包含 app_console.h 且设置 APP_DEVICES=1 才链接设备扩展。显式 console_open 请求已有 grant，
无设备支持返回 ENOSYS，无授权 EPERM；部分取得失败会回滚，不能从 INFO 推导权限。
write/number/clear 提供文本输出；readline 使用调用方缓冲区，capacity 为 2..4096，最多写 capacity-1 字符并保留 NUL。
readline 是单线程阻塞轮询，支持 Shift/Caps、Backspace/Enter。EOVERFLOW 清空当前行并重置解码器，返回给应用决定提示。
console_info 返回实际 queued/lost 和固定文本几何信息；不会编造硬件统计。

运行时设备/协议错误使 Console service 失败停止，后续绘制不再使用它；close 仍清理其他设备并传播原错误。
即使应用忽略这个错误，正常成功终结也不会把失败隐藏成成功。普通无输入与输入溢出不属于永久失败。

旧 `int ael_main(struct ael_heap *)` 不删除。新程序通过单独 app-entry.o 选择新入口；
旧程序不链接该对象。不要同时定义两个入口或直接读 ael_runtime 来替代快照。

本阶段是静态链接的源级 C 接口；导出头、对象和库须来自同一 SDK。更新 SDK 应重编译应用，不承诺跨版本二进制混配。
