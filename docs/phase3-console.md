# Console 应用与交互

应用位于 apps/console，入口 `ael_main` 由现有 ael_start 调用。
它不链接内核对象，不依赖私有头，不复制内核自带 Console。core 只调用 ael_* 模块接口。

```sh
make console                 # build/ael-console.elf（first-fit，Preemptive）
make build/console-best.elf   # best-fit，同样的 Console 源码
```

初次需要一个已构建的、匹配 Phase 7.3 的 Liberums 内核。两个 checkout 同级时：

```sh
make -C ../Liberums -j2
make -C ../Liberums run-external-console \
  CONSOLE_ELF=../Aelthuvor/build/ael-console.elf
```

该目标默认 GTK，并会准备 Liberums 自己的外部用户程序，但 `-initrd` 指定的映像必须是 Aelthuvor 的文件。
修改 Aelthuvor 后只执行 `make console` 再运行上述目标即可；无需重新链接内核。
如果路径不在同级目录，CONSOLE_ELF 使用绝对路径；不要误用内核 build/external/console.elf。

external-console 是 IRQ0 Preemptive Profile，并授权 VGA RW 和 PS/2。普通 external 不授予设备。
不需要强行在 Cooperative 环境运行交互式键盘程序；原 Cooperative first/best 内存示例单独保留。

## 行编辑与命令

US translated set-1 支持字母、数字、常用标点、双 Shift、Caps Lock、Backspace、Enter。
Caps Lock 按下时只切换一次，释放 Shift 不影响另一 Shift。E0 扩展键（如方向键）和 Pause 序列被忽略。
没有左右移动、历史或任意快捷键。发生队列溢出时重置 Shift/Caps/prefix，丢弃当前编辑行并提示重新输入。

输入缓冲区是 Heap 分配的 64 字节，最多 63 个可见字符，始终保留 NUL。满行后忽略更多字符，行首 Backspace 无操作。
屏幕维护 row/column/attribute，自动换行、滚动并用受保护 CURSOR 调用更新光标。行编辑有界，不会覆盖 prompt。

| 命令 | 实际行为 |
| --- | --- |
| help | 列出固定命令 |
| clear | 清可见文字并重新显示 prompt |
| about | 说明独立 Ring 3 LibOS |
| status | 显示 runtime 查询的 ABI/features、Heap policy/pages/live、VGA 本地状态和 Keyboard STATS |
| memtest | 实际分配 96 和 4096 字节，写读校验，free/trim 后核对 live 和状态 |
| exit | 释放键盘、VGA、输入缓冲区并正常结束域 |

未知命令给出反馈。没有虚构 CPU/硬件信息，没有文件系统或 ELF 加载器。
memtest 的普通分配不足会显示 FAIL 并保留输入缓冲区；完整性或分配器状态错误采取 exit(96)。

启动按 VGA acquire → map → keyboard acquire → 输入缓冲区分配进行，失败会回滚已经取得的部分。
设备错误后主循环停止，释放其余仍有能力的设备和缓冲区；更严重的硬件故障由内核收尾。
`console-test.elf` 才有私有诊断请求变量用于 #UD；生产 ELF 没有该控制分支。
观察点只供 GDB 读取用户状态，不授予权限，也不写内核输入队列。

应用退出状态：97 为初始化失败，98 为运行期间的设备/协议失败，99 为清理失败；
它们与 runtime 的 93～96 状态区分。故障异常由内核记录，不能当作正常 exit。
