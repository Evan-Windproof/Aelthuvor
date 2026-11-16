# Phase 2 实际验收与边界

本记录来自 2026-11-16 本轮机器实际执行。仅修改 Aelthuvor；未修改、提交或推送 Liberums。
Aelthuvor 起点与当时远程 main 均为 `b10d337d254b18e98204990203a77a215c82c568`。
Liberums 实测 checkout 是 `cdd83e7ff7d6ca55e05f3433945e0bbee2c4ca9c`；与 Phase 6 基准
`85a493466afe2038ca22a9870f6f9ce37dd7f3a8` 的差异是审查文档/证据/测试，公开头未变。
SDK 固定到后者，四个头均由 git show 获取并核对 SHA256。

## 工具与复现命令

Debian 13/x86_64，GCC 14.2.0、Binutils 2.44、GNU Make 4.4.1、Python 3.12.14，
QEMU 10.0.13（TCG/qemu32、32 MiB、单 CPU）、GDB 16.3（i386 + Python）。
标准 Debian 系统可安装 `build-essential binutils python3 qemu-system-x86 gdb`。
本轮没有改系统包配置；从签名验证的 Debian trixie/trixie-security HTTPS 索引下载 25 个包，
APT 保留签名、TLS 和包哈希验证，dpkg-deb 解包到 `/workspace/aelthuvor-tools/root`。
最初启动失败是解包后的 BIOS 搜索路径问题；将 seabios 文件链接进本地 QEMU data 目录后重试成功。

本轮工具目录可由以下步骤重新准备（已有系统工具时无需重复）：

1. 在仓库外建立 APT lists/cache 目录，sources 使用
   `https://deb.debian.org/debian trixie main` 和
   `https://security.debian.org/debian-security trixie-security main`，指定
   `signed-by=/usr/share/keyrings/debian-archive-keyring.gpg`。
2. 用独立 APT_CONFIG 覆盖 Dir::Etc::sourcelist/sourceparts、Dir::State::lists、Dir::Cache::archives；
   执行 `/usr/bin/apt-get update`，随后以
   `-o Debug::NoLocking=true --download-only --no-install-recommends -y install qemu-system-x86 gdb`
   下载。NoLocking 仅避免只下载时申请系统 dpkg 锁，不关闭任何完整性验证。
3. 对下载的 deb 执行 `dpkg-deb -x PACKAGE /workspace/aelthuvor-tools/root`。
   本地包装器设置该 root 的 `usr/lib/x86_64-linux-gnu` 为 LD_LIBRARY_PATH；
   QEMU 用 `-L ROOT/usr/share/qemu`，将 `ROOT/usr/share/seabios/*` 链接入该目录；
   GDB 用 `--data-directory=ROOT/usr/share/gdb`。
4. 先运行 `qemu-system-i386 --version` 及 `gdb -batch -ex 'python import gdb'` 检查。
   包、工具、包装器均在 Git 仓库外，不作为项目依赖或构建产物提交。

独立参考内核克隆在 `/tmp/aelthuvor-liberums-reference`，detached 到上述审查提交。
其源码保持 git status 干净，只有忽略的 build/ 输出。用户可换成任何独立路径：

```sh
make clean
make -j2
make test
make integration LIBERUMS=/tmp/aelthuvor-liberums-reference \
  QEMU=/workspace/aelthuvor-tools/bin/qemu-system-i386 \
  GDB=/workspace/aelthuvor-tools/bin/gdb
python3 tests/check_sdk.py --source /tmp/aelthuvor-liberums-reference --pinned-only
make -C /tmp/aelthuvor-liberums-reference test-phase6 \
  QEMU=/workspace/aelthuvor-tools/bin/qemu-system-i386 \
  GDB=/workspace/aelthuvor-tools/bin/gdb
git diff --check
```

## 通过项目

| 检查 | 本轮结果 |
| --- | --- |
| 清理后独立构建 | 静态 ELF32 + 静态库，默认仍为 cooperative first-fit |
| 四种 Profile/策略组合及诊断变体 | 编译链接通过 |
| SDK 文件集合、版本、SHA256、固定 Git 来源 | 通过（四个公开头） |
| SDK 漂移拒绝 | 4 项通过，无跳过 |
| 原有宿主回归 | 77,438,415 条断言，ASan/UBSan 无报告 |
| 新撤销/Profile 宿主测试 | 7,789 条断言，含 1,000 轮确定性生命周期序列，ASan/UBSan 无报告 |
| Aelthuvor 自有集成套件 | 13 QEMU guests / 26 轮生命周期，通过，无跳过 |
| Liberums 原有 Phase 6 专项 | 10 QEMU guests / 20 轮生命周期，2 个 native boundary suites，通过 |
| 仓库检查 | diff --check 通过；不包含工具、缓存或 build 输出 |

新宿主测试涵盖 ABI 1.2/1.1、错误 major/minor、缺特性、Profile 不匹配、POLL/INFO/ACK、
高位 Request ID 和 generation、错误身份、重复 ACK、过期 ACK、INFO 失效、释放取消事件、
消失的强制完成、空/活跃 Arena、ESTALE/EBUSY、协议错误、非消费重复事件与队列边界。
两种策略都有实际空洞选择检查；循环序列反复申请、读写、free、ACK/trim 和资源复用。
公共 alloc/free/trim/destroy 的 fail-stop 路径用 mock exit + setjmp 验证不会返回应用或继续 ACK。
宿主 reclaim 会实际 munmap，以检测后续错误 payload 访问；它仍不是内核硬件验证。

## Aelthuvor 真实 guest 覆盖

Cooperative first/best、跨域读取故障、只读代码写故障、带资源 exit、运行时 ABI 拒绝、
加载器 major/feature 拒绝、部分装载 OOM 回滚，保留原九类覆盖。
再增加 preempt-first、preempt-best、历史 ABI 1.1 原源码/SDK 映像、cooperative note 在 preempt 下拒绝。
每个 guest 均运行两轮，检查生命周期页数恢复。

硬件断言保留了真实 CPL3/SS、独立 CR3、TSS 可信栈、LOAD/PTE 权限、未映射空洞、
BSS/栈零初始化、资源 PFN/完整 generation/owner epoch/rights、1024 字实际物理数据、
Peer 无共享映射，以及释放/故障后资源页、动态页表、image 和栈的清零/回收。

抢占另要求 CPU-bound 循环中 EIP 处的 vector 32 IRQ、IF、无 yield 的 A、Peer 实际获得 CPU、
精确 GP/ESP/SS 及算术标志/DF 恢复、后续数据自检成功、主动释放和两轮回收。
Peer 可能在应用内存检查点前运行；因此从启动起观察，不假设固定调度时机。
初版顺序脚本错过已退出 Peer，跨到下一轮后产生虚假的寄存器不一致；修正为自动观察并加轮次断言后，
完整恢复断言通过。没有修改内核或取消该断言。

`make abi11` 从固定 Phase 1 提交和旧 SDK 重建，provenance.json 记录本轮 ELF 哈希。
不是新 SDK 构建后只改 note，也不声称保存了上一轮相同字节的编译产物。

## 集成入口变更与等价性

旧 Aelthuvor integration.py 要求工作目录头逐字节相等，不能表达历史运行兼容。
Liberums 历史 tests/libos.py 又固定 runtime.version==0x10001，会拒绝合法的 1.2 返回值。
新 tests/guest_audit.py 以其 GPL-3.0-or-later 审计为基础，保留隔离、PTE、资源和清零断言，
将版本判断改为 major/minor/features，并加入 Profile、真实 IRQ 恢复和历史映像检查。
共享的 Kernel GDB 辅助函数只读导入，不修改任何历史测试。
固定来源的字节校验仍保留，独立于运行兼容判断。旧入口不再作为新 ABI 的成功判据。

## 未执行与协议限制

**真正的 Aelthuvor 外部撤销 E2E：未执行。** 内核已有 phase6=ack/force/release/exit-pending
只为内置诊断程序产生事件，并没有对外部 LibOS 的一般压力触发入口。
本轮没有修改内核或用调试器伪造用户协议成功。内核专项通过证明的是内核机制，
宿主测试通过证明的是 Aelthuvor 客户端，两者不等同于端到端撤销已通过。
因此 Phase 2 是有明确验收缺口的阶段成果，不能声称全部目标完成。

建议未来最小测试接口：独立内核的显式诊断启动选项，在外部域实际取得页面后，
通过内核验证的 handle/epoch 发出空 Arena、活跃 Arena、期限超时事件；暴露只读 checkpoint，
供 GDB 检查 ACK/exit/#PF 和两轮清零回收。不要开放能跨域伪造权限的生产 syscall。
是否增加该接口属于未来 Liberums 任务，本轮未实施。

强制完成后的事件消失；QUERY 检查不能保证下一条普通指针访问安全。
没有 tombstone、pin/租约或用户异常恢复时，不提供任意活跃内存的透明恢复。
也未做真实硬件、SMP、线程、设备/DMA、POSIX 或新云任务恢复验收。
