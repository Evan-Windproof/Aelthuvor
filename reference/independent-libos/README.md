# Independent LibOS Reference

GPL-3.0-or-later。该程序是另一个最小 LibOS，不是 Aelthuvor 应用。
不包含 aelthuvor 头或库。启动、runtime、syscall、对象池和 linker 都在本目录独立实现。

默认 SDK=../../sdk/include 只提供受控的 Liberums 公开 ABI 头：核心来源
85a493466afe2038ca22a9870f6f9ce37dd7f3a8（ABI 1.2），可选设备头来源
5d9c8f35a4ae54ef08b93b7f4d5fecd9e350acbf；哈希见 ../../sdk/manifest.json。
官方 SDK 头的 GPL-3.0-or-later 许可保持，未重新编写相似头冒充官方来源。
本程序只使用四个核心头，不需要设备扩展。

```sh
make SDK=/absolute/path/to/public-sdk/include OUT=/tmp/reference-out
```

需要 GCC/Binutils/Make；freestanding -m32 无宿主 libc，输出 /tmp/reference-out/reference.elf。
内核 ABI note 要求 1.2、PAGES/ELF32/LIMITS/PREEMPTION=0x35。
一次实际 CLAIM/MAP 建立一个 4096B 页，在窗口末页组织 64×64B 对象，最高空槽优先、LIFO 复用。
应用申请全部对象、校验数据、执行 CPU work、归还并退出。错误保留待清理记录或结束本域。

启动时向 Phase 8 内核提供 -append multi-external -initrd reference.elf；
逗号列表可以同时给出 Aelthuvor ELF，两者分别占一个域、CR3 和私有栈，不需要彼此启动或继承。
内核保护资源，pool.c 选择策略，application.c 消费接口。

没有通用 Heap、设备驱动、ACK 客户端或透明撤销恢复。diagnostic-app.c 仅用于显式负测试，
可用 APP_SOURCE=diagnostic-app.c 并选独立 OUT 构建；不要把其私有调试输入当作 IPC API。
详见 ../../docs/independent-libos-reference.md。

对象指针只在 take 到 give/close 之间有效；地址复用后裸指针不能区分新旧对象。同域 C 不是内存安全沙盒。
