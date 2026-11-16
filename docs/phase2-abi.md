# ABI 1.2 与历史兼容

SDK 的唯一来源是 Liberums Phase 6 实现提交
`85a493466afe2038ca22a9870f6f9ce37dd7f3a8` 的 `include/abi/`。
四个头（syscall/resource/elf/event）逐字节 vendoring；manifest 固定完整 SHA 和各头 SHA256。
没有私有内核头参与编译。`make check-sdk` 校验文件集合、哈希、版本；四类漂移负测试必须失败。

ABI 返回 0x00010002。major 相等、运行时 minor >= 所需 minor、required features 全部存在，
才兼容。运行时先 ABI_QUERY=3，再 RESOURCE_LIMITS=4；不满足时 exit(93)。初始化失败 exit(94)。

| 构建 | ELF note major/minor | required | 内核 profile features |
| --- | --- | --- | --- |
| Cooperative | 1 / 2 | 0x1d | 0x1f |
| Preemptive | 1 / 2 | 0x75 | 0x77 |
| Phase 1 历史 ELF | 1 / 1 | 0x1d | 0x1f（ABI 1.2 cooperative） |

0x75 = PAGES(1) + ELF32(4) + LIMITS(16) + PREEMPTION(32) + REVOCATION(64)，不含 COOPERATIVE。
TRANSFER(2) 非分配器所需。旧 syscall 0..4、16..23、权限位与寄存器 ABI 不变。
新增 24/25/26 的标量协议见 phase2-revocation.md。

ELF 加载器先检查 PT_NOTE，不满足返回 -93，不创建用户域。runtime-abi 变体故意让 note 合法，
运行时要求 major=2，因此可加载但 exit(93)。集成明确区分两条路径。

来源完整性不是运行兼容性。`check_sdk.py --source PATH --pinned-only` 只读取固定 Git 对象；
不要求当前内核公开头与旧 SDK 相同。省略 --pinned-only 则额外检查工作目录，适用于有意更新 SDK。
集成保留固定来源检查，运行判断由 loader、runtime 和 GDB 的 major/minor/features 断言完成。

`make abi11` 用 git archive 从 Phase 1 完整提交重建原始源码和 ABI 1.1 SDK，以 -B 禁止复用旧输出。
其 provenance.json 固定历史来源、archive SHA256、本轮 ELF SHA256 和原 SDK manifest。
新旧源码从未混编；该映像在新 cooperative 内核上通过真实 CPL3、内存和回收检查。
这证明原始 ABI 1.1 程序重建后的二进制兼容，不声称是保存自上一轮机器的同一个文件。
需要本地包含该历史提交；浅克隆缺少对象时应明确 fetch 历史，不能静默换成新源码。
