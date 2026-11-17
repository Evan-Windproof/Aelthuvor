# Phase 4 实际验收

日期：2026-11-17。Aelthuvor 起点及开发前远程 main：
`46c7284325cdb04f7f6fd6a96377cf8ab9721a67`。固定只读参考内核及当时远程 main：
`449c78f9b12535cded2bca575cd44e0b65a0953b`。
本轮未修改、提交或推送 Liberums，公开 SDK 六个头及 manifest 与 Phase 3 完全相同。

## 当前机器与命令

复用已有 GCC 14.2.0、Binutils 2.44、GNU Make 4.4.1、Python 3.12.14，
QEMU 10.0.13 / TCG / qemu32 / 单 CPU / 32 MiB、带 i386/Python 的 GDB 16.3。
没有重新配置或发布云环境。普通 Linux 可用同类系统工具，无需特殊开发者路径。
本轮工具包装器在 /workspace/aelthuvor-tools/bin，参考 checkout 在 /tmp/aelthuvor-liberums-reference。

```sh
make clean
make -j2
make profiles
make test
make sdk-tests
make phase4-integration LIBERUMS=/tmp/aelthuvor-liberums-reference \
  QEMU=/workspace/aelthuvor-tools/bin/qemu-system-i386 \
  GDB=/workspace/aelthuvor-tools/bin/gdb
make integration LIBERUMS=/tmp/aelthuvor-liberums-reference \
  QEMU=/workspace/aelthuvor-tools/bin/qemu-system-i386 \
  GDB=/workspace/aelthuvor-tools/bin/gdb
make console-integration LIBERUMS=/tmp/aelthuvor-liberums-reference \
  QEMU=/workspace/aelthuvor-tools/bin/qemu-system-i386 \
  GDB=/workspace/aelthuvor-tools/bin/gdb
python3 tests/check_sdk.py --source /tmp/aelthuvor-liberums-reference
git diff --check
```

初次取得新 Phase 8 checkout 时显式构建一次匹配内核；后续应用开发/测试开始前记录内核 SHA 和 mtime，
冻结内核后，脚本再执行仓库外 SDK/参考 LibOS 的实际构建、修改和重建，
随后执行这些 ELF；完整新套件结束后二者不变。没有把用户程序链接入内核。最初构建内核和更新应用不是同一件事。

## 通过的检查

| 范围 | 实际结果 |
| --- | --- |
| clean / 默认构建 / profiles / 旧 Console | 通过，原入口与行为保留 |
| 核心/扩展来源、SHA256、文件集合 | 六个头通过；来源/版本未变；10 个漂移负测试仍通过 |
| Phase 1 ASan/UBSan | 77,438,415 条断言通过 |
| Phase 2 ASan/UBSan | 7,789 条断言、1,000 轮生命周期序列通过 |
| Phase 3 ASan/UBSan | 853 条断言通过 |
| 新 Application SDK ASan/UBSan | 375 条断言通过 |
| 独立 reference pool ASan/UBSan | 288 条断言通过 |
| 隔离构建/静态链接审计 | 外部 SDK 应用、独立 LibOS、入口冲突、设备可选性、单独源码重建与策略切换均通过 |
| 原 memory integration | 13 guests / 26 完成 lifetimes，含原 ABI 1.1 映像、双策略、双 Profile、安全断言 |
| 原 Console integration | 9 guests / 11 完成 lifetimes + 1 个持续交互会话，原断言保留 |
| 新 Phase 4 integration | 13 guests / 18 完成 lifetimes，无跳过 |

计数不把宿主序列当成硬件测试，也不把持续 Console 计作已退出生命周期。
程序故障属于预期隔离场景，不是被跳过的失败。所有成功结论均来自本轮实际执行。

Application SDK 宿主覆盖绑定、重复绑定、快照、错误传播、OOM、非法/重复 free、trim、log 额度、yield、
成功退出、泄漏状态、map+rollback 双失败留下的无 Arena claim、显式 exit、closed context、sticky heap fail-stop、设备无 grant、部分初始化回滚、
可选服务、readline Shift/Caps/编辑/长度/溢出/句柄失效与服务清理。严重失效后不会继续返回旧指针。
参考 pool 宿主覆盖一页 64 个对象、地址顺序、耗尽、非法/重复 give、LIFO、map 失败、release 失败保留和失效退出。

## 消费者角度的独立性证据

sdk_builds.py 将已导出的 SDK 和应用模板复制到仓库外 /tmp/libos-phase4-consumer-*，构建后实际改写应用消息再重建。
本次目录记录在 phase4-build-results.json；只保留最终 ELF 到仓库忽略的 build/。

- 修改前 ELF：`5c6083076e297e91aaabf7bd254a299923ba4e3ccbce7b61b233b6b6a3cbc231`
- 修改后 ELF：`8fd3d3adb8b2ba24b5e0a7d326e881e1375893c2255df4dd4d1cbe89701407d5`

两次 ELF 不同，LibOS core 对象、静态库和导出对象的 SHA/mtime 不变。
相同 OUT 切换 first→best→first 后恢复正确 runtime；两个入口同时链接会因重复 ael_main 被拒绝。
Hello/Memory ELF 不含 VGA/Keyboard/text 符号，两个 archive 不含应用或启动 main。
新公开头不引入 src/ 或 kernel 私有类型。

独立参考的隔离目录只有其自身源码和公开 abi SDK。compiler .d 没有 Aelthuvor/内核私有头，
link map 没有 libael，nm 没有 ael_* 或未解析符号。实际 QEMU 使用该隔离编译的 ELF。
这不是只在原仓库内修改路径后假称独立，也不是把内核自带 worker 换名。

## 新 QEMU/GDB 覆盖

| Guest | 完成生命周期与证据 |
| --- | --- |
| Hello | 1；真实 CPL3/CR3/栈、ABI 查询、串口消息和 EXIT |
| 外部 SDK 模板 | 1；运行修改后的实际独立构建产物，串口出现其独有消息 |
| Memory first / best | 各 1；真实空洞选择、普通资源页写读、IRQ0/恢复和清理 |
| 独立参考 | 1；一页 64 对象，末页 VA、实际字节、CPU 抢占/恢复、pool 归还 |
| 双 LibOS 正常并存 | 2；两个不同 CR3、映像/栈 PFN，分别执行自身策略并恢复资源 |
| 双 LibOS 句柄对抗 | 2；真实旧 generation ESTALE、对方活跃 handle QUERY EPERM，其余执行不受影响 |
| 双 LibOS 物理访问对抗 | 2；真实 #PF vector 14 / error 5 / CR2=对方 PFN，EIP 为参考程序实际 load；Aelthuvor 继续退出成功 |
| SDK 应用泄漏 | 1；成功 main 留活跃分配导致 exit(95)，内核最终回收 |
| SDK 应用异常 | 2；Aelthuvor #UD，独立参考继续执行并正常退出 |
| Profile 拒绝 | 1 个成功域；cooperative 模块 -93，后续参考模块仍成功运行 |
| SDK Console | 1；QMP→PS2→IRQ1→readline，help/clear/about/status/memtest/exit，Heap 与设备清理 |
| SDK cooperative Hello | 2；原 external 合作式路径运行，证明新 API 不强制设备或 Preemptive |

共 13 次 QEMU，18 个完成域生命周期；装载拒绝不计成功域。
所有新 multi guest 使用真实逗号 Multiboot ELF 模块列表，不提供用户 spawn 服务。
普通并存使用生产 reference application，错误场景才显式选择 diagnostic-app。

GDB 检查 ELF LOAD 文件/BSS、RX/R/RW、零栈、不同页目录/PFN、supervisor 内核、TSS 与 IF/IOPL；
资源 owner ID/epoch、rights=47、generation、单一 binding 与不同 RAM VA；
Aelthuvor 0x09000000 对比参考 0x0900f000，各自实际 4096 字节模式。
CPU-bound 程序在真实 vector 32 IRQ 后恢复，通过后续数据和正常 EXIT 检查。
结束检查所有槽 UNUSED、context/内核栈/image/stack/页表/RAM 清零，资源=0、事件=0、设备/队列=0，
实际 free count 回到 multi_free_before。

新 SDK Console 只使用公开应用接口；通过真实键盘编辑输入命令，实际 memtest 后 live=1 只剩输入行，
退出时设备、行缓冲和 RAM 都清理。旧 Console 整套 CAPS/长行/滚动/设备撤销断言另外保留并通过。
[本轮新 SDK Console 渲染截图](../build/phase4-sdk-console.png)在本机 build/，clean 后需重跑生成。

调试器只写参考诊断应用的私有标量输入，没有改 PTE、内核 owner、授权或返回值。
不同 CR3 可在同一个线性 PC 放不同函数，因此 Python 观察回调按当前域过滤，再执行原 CPL3/CR3/栈断言。
初版误命中造成的失败已修正并完整重跑，未移除安全断言。隔离构建先清理自己的生成目录，避免旧输出时间戳冒充重建。

## ELF 来源与 SHA256

内核 SHA256：`5120cbeec5151ef365b2548338c3922481b9baf610e1ee10ea82d62223e207b3`，对应固定 Phase 8 commit。源码 git status 保持干净。
以下是本轮实际执行的独立 ELF（包含调试路径，不承诺不同目录/工具链产生相同 hash）：

- build/apps/hello/app.elf：`6a6e639b8452a418a65a5d9a547be569b0d8812e5d32d38d8eaa8d25e0f0ddae`
- build/apps/memory/app.elf：`9c602b943bc4e2ccacec4df8cc6ee158dae9713202a3cf41f0aea747494451b9`
- build/apps/memory-best/app.elf：`489a89cf93f6fc305e07754ffbfcf83463f90b072eab3366434ad5ef0bb8b716`
- build/independent-isolated.elf：`1dec6b4ee5e551b996ce4bc7ecd465590c797acf4b08596bc5a00132ae070eeb`
- build/independent-diagnostic/reference.elf：`8bbe312188b8f29564b6516843dd41e34a4f59b94e5e539ba54c41e43c627b9a`
- build/third-party-app.elf：`8fd3d3adb8b2ba24b5e0a7d326e881e1375893c2255df4dd4d1cbe89701407d5`
- build/apps/console/app.elf：`e732790e1ba9f6d752320180d9b7c3a0e53b24e576676498ed23a12ecbe43e53`

机器可读记录为 build/phase4-results.json、phase4-build-results.json；GDB/串口日志在参考内核忽略 build/ 下。
源码、构建和日志没有混合提交；仓库不包含编译器、下载工具、缓存或生成 ELF。

## 尚未验证与后续资料

WSL2 GTK 实体键盘/窗口焦点仍待用户本地人工验收，云端 QMP/显存/截图不能替代。
外部普通活跃 RAM 的通用撤销 E2E 仍未补齐。API 继承 fail-stop 与 MMU 隔离边界，没有透明恢复承诺。
不宣称验证了超过四域、SMP、多线程、通用 IPC、动态加载、文件系统或 POSIX。

未来建议 Liberums Phase 10 在自身仓库重新审查并接收：中立接口笔记、独立参考源码与 README、
兼容矩阵、构建/运行说明和失败/回收实验。当前只提供可复现实验依据，不预先修改其内核架构。
本轮到 Phase 4 停止。
