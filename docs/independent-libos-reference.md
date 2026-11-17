# Independent LibOS Reference

参考目录 reference/independent-libos 是 GPL-3.0-or-later 的独立教学实现。
它只借用固定来源的 Liberums 公开 ABI 头，不包含 aelthuvor 头，不链接 libaelthuvor.a，
也不使用 Aelthuvor 的 runtime、pool、Heap、Console、设备包装或应用上下文。

三层在源码中明确分开：

1. start.S / syscall.c / runtime.c：自己的入口、C 初始化、标量 int 0x80、版本/额度检查、exit。
2. pool.c / pool.h：用户态固定对象池。只 CLAIM 一页，映射资源窗口最后一个 VA；metadata 在自身 BSS。
3. application.c：仅通过自己的 take/give 消费对象，写读校验、耗尽/重用检查、CPU work 和正常退出。

pool 使用 64 个 64B 对象的 free stack，取最高空槽、free 后 LIFO 复用，没有可变块拆分/合并。
它不是把 Aelthuvor 改名，源码和策略独立。内核仍负责物理页所有权与保护。
错误 map 会尝试 release，失败清理保留句柄；QUERY 失效退出 86，成功程序 cleanup 失败退出 87。
程序不实现 Visible Revocation ACK、设备或通用堆，不承诺强制撤销后的任意指针恢复。

```sh
make independent-libos
# 不使用 Aelthuvor 构建规则，也可以从该目录直接运行：
make -C reference/independent-libos SDK=/path/to/public-sdk/include OUT=/tmp/reference-out
```

SDK 仅需 abi 公开头；其来源、哈希和许可证见 sdk/manifest.json 及目录 README。
独立构建测试会复制参考源码和仅含 abi 的 SDK，检查 compiler .d、link map、nm 和未解析符号；
随后实际运行这个隔离编译的 ELF，而不是换成 Aelthuvor 或内核自带 worker。

使用已经构建的 Phase 8 内核（路径按实际替换）：

```sh
qemu-system-i386 -machine pc -accel tcg -cpu qemu32 -m 32M -smp 1 \
  -display none -serial stdio -monitor none -nic none -no-reboot -no-shutdown \
  -kernel /path/to/Liberums/build/liberums.elf -append multi-external \
  -initrd /path/to/Aelthuvor/build/apps/memory/app.elf,/path/to/Aelthuvor/build/independent-libos/reference.elf
```

两 ELF 各自运行在一个域，互不共享地址空间、不启动对方。相同内核不包含 LibOS 品牌分支。
无设备 grant；若将设备应用加入列表，须按模块序号显式指定 multi-vga/multi-keyboard，设备仍独占。

diagnostic-app.c 只用于负测试，是明确选择的另一应用：先用真正旧句柄验证 ESTALE，
接收测试私有的对方 handle/PA 标量，然后实际发出未授权 QUERY 或读操作，分别验证 EPERM 与 #PF。
调试器只提供输入，不伪造返回值、PTE 或所有权。生产 application.c 没有这个控制接口。
