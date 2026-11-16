# Console 与 Heap 的协作

Console 是应用，ael_alloc/ael_free 和 page pool 是 LibOS。输入缓冲区由应用借用，
分配器仍负责 first-fit/best-fit、16 字节对齐、分裂、合并、quota、回滚和空 Arena 回收。
本阶段没有用设备 syscall 替代分配器，也没有把设备 handle 塞入 RAM pool。

RAM Window 为 [0x09000000,0x09010000)，VGA 固定在 0x09400000。
前者背后是真实的普通资源页与 generation，后者映射设备物理页 0xb8000。
VGA 不计入 pool.owned、heap.live 或 trim/destroy，不由 RAM release 清零。
Keyboard 只有受保护队列和独立句柄，不给应用映射缓冲页面。

Console 状态、设备句柄和解码器在 image/BSS；输入行在实际 Heap 中，字符单元在设备页。
正常启动后 live=1。memtest 暂借两个缓冲区，写入确定性数据并读回校验，再 free/trim；
成功要求 live 恢复到原值。关闭时释放输入行，最终 RAM quota 回收为零。

每次 Console step 都先做 ael_heap_check。原有 active RAM 撤销和失效检测继续使用 sticky failure 与 exit(96)，
不会在页已知失效后继续访问输入指针。单 CPU/每域单线程下，IRQ 只暂停/恢复同一 C 调用栈；
没有同域异步回调去修改半完成的块表。IRQ1 将原始字节放入内核队列，不直接修改用户输入行。

设备 grant 撤销不是普通页面 Visible Revocation。设备错误结束 Console；RAM 撤销仍遵循 Phase 2 协议。
当前普通页面强制完成后事件可能消失，查询也不能消除 TOCTOU；检查后访问仍可能 #PF。
本轮设备撤销实验证明不了普通活跃 RAM 的透明恢复，也不填补尚缺的外部 RAM 撤销 E2E。
