# Aelthuvor — a user-mode LibOS for Liberums

Phase 1：独立的 **C11/i686 Library OS**，以静态 ELF32 运行于 Liberums 的真实 Ring 3。
内核负责资源所有权、Secure Bindings、页表与隔离；Aelthuvor 自己选择虚拟地址、管理块和空闲空间。
本阶段使用 GPL-3.0-or-later，见 [LICENSE](LICENSE)。

```text
应用 ael_main -> Aelthuvor runtime / first-fit 或 best-fit / 页面封装
                             | Ring 3
                 公开 ABI 1.1 / int 0x80
                             | Ring 0
Liberums -> 所有者/代数/权限检查 -> PTE -> MMU 执行普通 load/store
```

源码、对象和库不复制进内核。仅 `sdk/include/abi/` 是受控 vendoring 的公开接口，
固定来源完整提交及 SHA256 见 [sdk/manifest.json](sdk/manifest.json)。
构建不需要 Liberums checkout；只有集成测试需要两个仓库。

## 工具与独立构建

实际验证版本：Debian 13/x86_64，GCC 14.2.0、Binutils 2.44、GNU Make 4.4.1、Python 3，
QEMU 10.0.13/TCG/qemu32、GDB 16.3（支持 i386 和 Python）。
宿主测试需要 Linux 的 memfd/mmap 和 GCC ASan/UBSan。

Debian/Ubuntu 可安装 `build-essential binutils python3 qemu-system-x86 gdb`。
freestanding `-m32` 编译，不需要 32 位宿主 libc；也可传 `CROSS_COMPILE=i686-elf-`。
本次云环境已有仓库外工具：`export PATH=/workspace/.liberums-tools/bin:$PATH`。

```sh
make clean
make                  # build/libaelthuvor.a + build/aelthuvor.elf（默认 first-fit）
make test             # 宿主 ASan/UBSan + SDK 漂移拒绝
```

示例静态链接自己的入口/runtime、`examples/memory_demo.c`、共用行为检查与 `libaelthuvor.a`。
应用可实现 `int ael_main(struct ael_heap *heap)`，用自己的应用对象替换 demo 和测试对象：

```sh
# 应用使用与 Makefile 相同的 freestanding flags 编译后：
ld -m elf_i386 -nostdlib --build-id=none -T linker.ld \
  build/startup.o build/first/runtime.o my_application.o build/libaelthuvor.a -o application.elf
```

## 与独立内核集成

使用支持此 SDK 的 Liberums 提交，见 manifest；通常克隆到同级 `../Liberums`。
路径可以覆盖，不依赖开发者机器的绝对位置：

```sh
make check-sdk SDK_SOURCE=../Liberums   # 核对固定提交、工作目录头文件与 SHA256
make integration LIBERUMS=../Liberums # 构建两端，9 个 QEMU/GDB 场景、18 轮生命周期
make -C ../Liberums run QEMU_APPEND=external \
  QEMU_INITRD="$(pwd)/build/aelthuvor.elf"
# 或从 Liberums：make test-libos AELTHUVOR=../Aelthuvor
```

只有 `external` 参数才启动外部映像。没有 shell，最终内核停止；前台运行按 Ctrl+C 退出 QEMU。
QEMU 的 initrd 是独立 ELF 的 Multiboot 模块；LibOS 不链接任何内核对象。
`make integration` 还生成 best-fit、跨域故障、代码写故障、带资源退出及运行时 ABI 拒绝的测试 ELF。

## API 与限制

- `ael_alloc(heap, bytes)`：16 字节对齐、每次 1..4096 字节；返回 NULL 时见 `heap->error`。
- `ael_free(heap, pointer)`：只接受精确活跃块起点，NULL 无操作；非法/重复释放返回 EINVAL。
- `ael_heap_trim/destroy`：trim 归还全空页；destroy 有活跃块时 EBUSY，失败保留可重试元数据。
- 页面接口提供 claim/map/unmap/release；失败回滚，句柄保持完整 index/generation。

当前内核每域最多四个动态页，窗口 16 页。额度由 ABI 查询，物理不足仍可能提前失败。
只支持单 CPU、合作式执行；没有 POSIX、线程安全、文件系统、网络、动态链接或通用 libc。
原始指针在地址复用后没有代数，不能检测所有 use-after-free；用户越界仍可破坏同域数据，
不能据此赋予更多内核权限。普通 i686 无 NX，不提供完整 W^X。
Visible Revocation 和 CPU 抢占留待以后，本次停止于 Phase 1。

阅读：[架构](docs/architecture.md) → [ABI](docs/abi-compatibility.md) →
[内存管理](docs/memory-management.md) → [实际验收](docs/phase1-validation.md)。
生成 ELF、对象、日志均在被忽略的 `build/`；不提交工具链或临时输出。
