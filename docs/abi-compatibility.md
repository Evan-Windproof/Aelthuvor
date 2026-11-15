# ABI 固定与兼容

Liberums 是规范来源。`sdk/manifest.json` 固定来源仓库、完整 commit、ABI 1.1 和三个公开头的 SHA256。
Aelthuvor 默认编译只使用这个受控副本，不使用相邻仓库 include/kernel 或任何私有结构。
`make check-sdk` 检查文件集合、哈希、版本；`SDK_SOURCE=../Liberums` 另外用 git show 核对来源提交，
并比较当前公开目录；集成时也拒绝工作目录 SDK 漂移。宿主负测试确认篡改、增删头文件、版本漂移会失败。

升级流程：先审查内核公开 ABI 变化并提交；再有意更新本仓库公开副本、来源 SHA 与哈希，
检查 ELF note/运行时要求，运行两边测试与集成。两个仓库各自提交，不能构建时隐式引用最新内核头。
来源提交保留在 manifest 而非硬编码运行路径；最终交付的 LibOS SHA 由 Git 记录，避免文档自引用。

## 两层检查

1. ELF 的 PT_NOTE 要求 major=1、minor=1，PAGES/ELF32/COOPERATIVE/LIMITS（0x1d）。
   加载器对不兼容要求返回 -93，不启动域。
2. 运行时第一条系统调用查询 `SYS_ABI_QUERY=3`：EAX=(major<<16)|minor，EDX=features。
   major 必须等于所需值，minor 不能更旧，全部 required feature 必须存在。
   随后 `SYS_RESOURCE_LIMITS=4` 返回 EAX=每域动态页额度、EDX=每资源绑定上限（当前均为 4）。
   额度不是空闲页承诺；查询不授予权限。初始化失败调用 exit(93)。

ABI 1.1 向后兼容 1.0，保留 0..3 和 16..23 的寄存器及语义，只新增查询号/特性。
要求 1.1 的程序会安全拒绝旧内核。不兼容变更必须推进 major；兼容新增推进 minor/feature。
这是实验 ABI，不保证永久稳定。

## 调用与资源

`int 0x80`：EAX=号/结果，EBX/ECX/EDX/ESI 为整数参数；除了 EAX 和规定的 EDX 输出，其他通用寄存器保持。
SDK 封装的 extra 指针只写用户本地内存，绝不传给内核。

| 号 | 操作 | 输入 / 输出 |
| --- | --- | --- |
| 0/1/2 | yield / exit / log_char | exit 的 EBX=状态；log 的 EBX=单字节，128 次额度 |
| 3/4 | ABI / 资源额度查询 | EAX/EDX 如上，无输入 |
| 16 | claim | EBX=rights；EAX=index，EDX=generation |
| 17 | claim_specific | EBX=PFN，ECX=rights；返回句柄 |
| 18 | map | EBX=index，ECX=gen，EDX=VA，ESI=R/W；返回 0 |
| 19/20 | unmap / release | EBX=VA / EBX=index、ECX=gen；返回 0 |
| 21 | query | EBX=index，ECX=gen；EAX=PFN，EDX=rights |
| 22/23 | transfer / receive | 沿用公开 SDK；Phase 1 分配器不使用转移 |

错误是负数：EPERM=-1、ENOENT=-2、ESRCH=-3、ENOEXEC=-8、EAGAIN=-11、ENOMEM=-12、
EBUSY=-16、EINVAL=-22、ENOSPC=-28、ENOSYS=-38、EOVERFLOW=-75、EPROTONOSUPPORT=-93、ESTALE=-116。
失败的 claim/query/receive 的 EDX=0。READ/WRITE/MAP/RELEASE/TRANSFER/QUERY 为 1/2/4/8/16/32。
本库申请 47（不含 TRANSFER），映射只申请 READ|WRITE。

安全绑定由内核核对域 ID/epoch、完整 generation、rights 后写 PTE。普通读写不再陷入内核，
但 MMU 仍检查当前 CR3 的 U/S/RW。释放先撤销所有别名并刷新 TLB，清零再归还；本库不自行写 PTE。
