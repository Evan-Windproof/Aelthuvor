# Phase 1 实际验收

本记录来自当前云机器实际编译、宿主 ASan/UBSan、QEMU 和 GDB。未将编译成功或串口标记等同于运行证据。
开发前 Aelthuvor 只有 README（`ba77e9121afadbe9f34db697f87bd683c2cf1c2e`），工作区干净。
Liberums main 已是 `bdee0ad` 的 Phase 5 基础设施，且包含指定 Phase 4 基准
`d3efffd359f3a25a7a28758c32622208ca64029a`；保留远程新提交，未回退。
开发前先运行其全套基准：原宿主 10,905 条断言、49 个 ELF 输入、103 个 QEMU 场景通过。

## 环境与命令

Debian 13/x86_64，GCC 14.2.0、Binutils 2.44、Make 4.4.1、QEMU 10.0.13、GDB 16.3。
QEMU 用 TCG/qemu32、32 MiB、单 CPU。工具通过签名验证的 Debian 包索引下载/校验后解包至
`/workspace/.liberums-tools`，保持 TLS、签名、包校验和；不进入任一 Git 仓库。

```sh
export PATH=/workspace/.liberums-tools/bin:$PATH  # 仅本次云环境需要
# Aelthuvor：
make clean
make
make test
make integration LIBERUMS=../Liberums
make check-sdk SDK_SOURCE=../Liberums
# Liberums：
make clean
make -j2
make test
make
make test-libos AELTHUVOR=../Aelthuvor
# 两边：
git diff --check
```

| 验收 | 实际结果 |
| --- | --- |
| Aelthuvor 独立构建 | 静态 ELF32 + 静态库，无未解析符号；代码超过一页，保留旧内核单页断言 |
| 宿主算法/包装测试 | 77,438,415 条断言通过，ASan/UBSan 无报告 |
| SDK 负测试 | 四类漂移均拒绝；正向哈希/版本与固定来源检查通过 |
| Liberums 全套回归 | 103 个 QEMU guest、49 个宿主 ELF 输入、原 10,905 条宿主断言通过 |
| 独立 LibOS 集成 | 9 个 QEMU guest，每个两轮，共 18 个生命周期；全部通过，无跳过 |

宿主用 Linux memfd/mmap 替代页机制，直接测试真实 memory.c/page.c/compat.c。
覆盖零/溢出长度、16 字节对齐、非法与重复释放、空闲块复用/分裂/合并、4096 字节整页、
配额耗尽、最小块全表、两种策略、每种 3000 步确定性随机序列及所有存活数据/地址区间检查。
错误注入覆盖 claim ENOMEM、map ENOMEM、全窗口 EBUSY、release 错误后的可重试状态，
并验证真实 wrapper 的 unmap/remap 与 trim 重试。ABI 测试含旧 1.0 客户端兼容、新 major/minor/缺特性拒绝。

## 真实 guest 与硬件取证

`make integration` 构建两边，但内核只读取独立 ELF/符号；用户构建只读取 vendored 公共 SDK。
内核侧 `tests/phase5.py --aelthuvor <build目录>` 使用 GDB 硬件断点验证：

- 入口/运行时/分配后 CS=0x1b、SS=0x23，CR3 属于外部域，ESP 在私有四页栈。
- 第一条调用是 ABI_QUERY=3，下一条是 RESOURCE_LIMITS=4；陷入时真实外层 CS/SS/ESP 在 TSS 可信栈。
- LOAD 字节、BSS 和栈初值、全部用户 PTE 的权限及未映射空洞；内核 PDE 仍 supervisor-only。
- 用户 block checks 在 CPL3 执行；分配后资源表中 owner/epoch/generation、rights=47、引用数与唯一 PTE 一致。
- 1024 个字均为用户写入的 `0xa3110000 | i`；同伴 CR3 中无此资源窗口映射。
- 正常 trim 后库/内核资源计数和动态页表归零；最终映像、栈、页表、资源页都清零变为 FREE，空闲总数恢复。

实际 first-fit 摘录（地址是本次构建值，不属于 ABI）：

```text
CPL3: EIP=0x08000000 ESP=0x083ffffc CR3=0x00143000
CPL3: EIP=0x08000010 ESP=0x083fff8c CR3=0x00143000
RESOURCE: PFN=0x155 PTE=0x00155067 data=0xa3110000
LIBOS: policy=0 live=1 VA=0x09000000 PFN=0x155 generation=4
```

best-fit 使用同一内核，GDB 看到 policy=1；用户自检证明 96 字节请求实际选择另一个空闲块。
原始输出在 `Liberums/build/phase5-aelthuvor-*-gdb.log`，不是只读串口就判定。

| 场景 | 期望并已核对的行为 |
| --- | --- |
| first / best | 两种用户策略检查、真实页读写、yield 同伴、主动归还、exit(0) |
| fault | 带活跃绑定读取同伴地址 0x08800000，#PF/error=4，仅本域 FAULTED |
| readonly | 带活跃绑定写用户代码 0x08000000，#PF/error=7，仅本域 FAULTED |
| exit-live | 绕过库 trim 直接 exit(0)，内核回收活跃绑定和资源 |
| runtime-abi | note 合法，运行时故意要求 major=2，正常 exit(93)，不申请资源 |
| bad-major / missing-feature | 修改 note 后加载 -93，外部域不激活，同伴正常 |
| oom | 真实部分装载取得五页后 ENOMEM，五页清零归还，失败前后空闲数相同，同伴正常 |

故障 GDB 核对 EIP 为外部 ELF 的真实故障标签、CR2 与错误码，不修改异常帧或跳过指令。
每个场景同伴 B 都在自己的 CPL3/CR3 正常运行并 exit(0)，两轮均一致。
完整原始 ELF 损坏 corpus、内核/跨域授权攻击、旧单页路径、Phase 2 initrd 保留仍由 Liberums 全套回归验证。

## 日志、发布与边界

Aelthuvor 的 build.log/test.log/integration.log 与 ELF/map/对象在忽略目录 `build/`；
Liberums 的 build.log/test.log 与每个 guest 的 GDB/串口日志也在其 `build/`。
开发前基准备份在 `/workspace/validation-logs/baseline`；临时日志和工具不提交。
runner 对非零返回、超时、断言失败都报错，取证后仅终止自己的 QEMU 进程组。

无未解释测试失败或已知页泄漏；双仓库分别提交与安全推送，最终报告核对远程 SHA。
固定内核来源见 sdk/manifest.json，避免写入本文件自身的提交 SHA。
尚未实现抢占、公平 CPU 撤销、异步 Visible Revocation、SMP、NX/完整 W^X、动态链接、POSIX 或通用 libc。
用户指针地址复用后的悬垂指针检测不在本分配器保证内；未进行真实硬件或新云任务恢复验证。
完成后停止，不进入 Aelthuvor Phase 2 或 Liberums Phase 6。
