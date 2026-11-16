# 可选设备 ABI 与 SDK 来源

ABI 仍为 major 1 / minor 2，没有新的 VGA/Keyboard feature bit。已有 syscall 和特性语义保持不变。

`sdk/manifest.json` 的顶层 commit/abi/sha256 描述四个核心头，固定来源仍是 Liberums Phase 6
`85a493466afe2038ca22a9870f6f9ce37dd7f3a8`。`extensions.devices` 单独描述
vga.h、keyboard.h，来源为 Phase 7.3 `5d9c8f35a4ae54ef08b93b7f4d5fecd9e350acbf`。
每份来源有仓库、完整提交和每文件 SHA256；这些文件由 git show 逐字节取得，不重写官方接口。

check-sdk 合并所有来源的文件集合，拒绝重复文件、非法文件名、非精确提交、缺失/新增/篡改文件和版本漂移。
`--source PATH --pinned-only` 从每个文件对应的 Git 对象核对来源；不加 pinned-only 则还核对工作目录。
来源完整性和运行时 major/minor/features 兼容是不同检查，ABI 1.1 历史映像仍从原提交及旧 SDK 重建测试。

## VGA

| 调用 | 标量参数与结果 |
| --- | --- |
| INFO 32 | EBX=device；EAX=columns\|rows<<16，EDX=mapping bytes |
| ACQUIRE 33 | EBX=device，ECX=READ[\|WRITE]；EAX=index，EDX=generation |
| MAP 34 | EBX=index，ECX=generation，EDX=VA，ESI=rights |
| RELEASE 35 | EBX=index，ECX=generation |
| CURSOR 36 | EBX=index，ECX=generation，EDX=cell 0..1999 |

VGA handle namespace 为 0x10001，VA 为 0x09400000。文本是 80×25 个 uint16_t 单元，
低字节字符、高字节属性。只有 4000 字节用于绘制；4 KiB 页最后 96 字节不是文字空间。
设备绑定不进入 RAM pool，不能传给 PAGE_QUERY/RELEASE，不能按普通 RAM 归还或清零。

ael_vga_probe 验证几何信息；acquire/map 验证权限、固定 VA 和本地状态，并把完整 generation 传给内核。
绘制函数限定单元/区间边界，scroll 仅搬移可见文本。绘制前以 CURSOR 解析 WRITE 能力；INFO 本身不能验证授权。
ESTALE/EPERM/ENOSYS 会失效本地 device 状态，清除 cells 指针，后续绘制不会触碰映射。
只读 MAP 可用，但文字绘制/硬件光标需要 WRITE。成功 release 后重复 release 返回 ENOENT，不再次 syscall。

## Keyboard

| 调用 | 标量参数与结果 |
| --- | --- |
| INFO 40 | EBX=device；EAX=capacity，EDX=scan format |
| ACQUIRE 41 | EBX=device；EAX=index，EDX=generation |
| POLL 42 | EBX=index，ECX=generation；EAX=0，EDX=原始字节 |
| RELEASE 43 | EBX=index，ECX=generation |
| STATS 44 | 同一 handle；EAX=queued count，EDX=lifetime lost |

Keyboard namespace 为 0x10002。probe 只接受 64 字节队列、translated set 1；不支持的格式安全拒绝。
EAGAIN 是无输入；EOVERFLOW 的 EDX 是丢失计数，重置解码状态并丢弃编辑行。
ESTALE/EPERM/ENOSYS 失效本地句柄，Console 结束当前域。其他协议/设备错误也由应用走清理退出路径。
Generation 和计数保存为 uint32_t，不把 EDX 高位当错误码。release 清队列并使本地句柄无效。

INFO 成功仅证明接口存在。没有可信启动 grant，ACQUIRE 仍返回 EPERM；文件名、ELF note 和欢迎文字都不是授权。
旧内核 INFO 返回 ENOSYS 时，不能假装获得设备。两个设备也不使用普通 RAM 的 POLL/INFO/ACK 撤销协议。

任何检查都不能 pin 映射；检查后被强制撤销仍可能在直接 store 上 #PF。此时内核隔离和最终清理本域，
不承诺总能在下一次访问前发现失效，也不提供透明恢复。
