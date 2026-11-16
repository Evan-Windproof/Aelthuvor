# 页面失效与应用指针

页面状态与应用分配不是一回事。`ael_page` 的 handle 是旧/当前身份，owned/mapped 是本地记账，
state 说明生命周期；Arena 块表和 heap.live 则说明应用是否仍有借出的原始指针。
owned=1 不能证明内核还承认该句柄。低层 page API 不应与同一 heap 的 Arena 绕过协调混用。

```text
FREE / RELEASED / ACKED / LOST --claim--> OWNED --map--> MAPPED
MAPPED --unmap--> OWNED
OWNED / MAPPED --event--> PENDING --ACK成功--> ACKED
OWNED / MAPPED --QUERY ESTALE--> LOST
OWNED / MAPPED --release成功--> RELEASED
PENDING --活跃/ACK错误--> sticky heap failure --> exit(96)
```

终态清除 owned/mapped/address、减少 pool.owned，保留旧身份供诊断。成功 claim 才能覆盖旧身份。
空 Arena 在确认归还或已失效后脱离页记录，下一次分配不会从旧块表返回地址。
活跃失效时不伪装 free，保留块表并锁住整个 heap（failure 不可清除），当前域必须结束。
不能通过重新 init 一个持有资源或活跃指针的 pool/heap 来恢复。

`ael_alloc/free/heap_trim/heap_destroy` 在任何元数据更改前同步检查；遇到 sticky 错误 exit(96)，
不返回普通错误让不知情的应用继续。显式 `ael_heap_check` 提供诊断返回值，调用方必须检查并终止。
release、map 回滚保留原有可重试记录；pending 页不允许普通 map/unmap/release。
同域代码仍能绕过 API，和 Phase 1 一样不属于内核信任边界。

安全保证的边界：QUERY 之后可能立刻被抢占，内核强制撤销，恢复后任何普通 load/store 都可能 #PF。
甚至尚未处理完块分裂时也可能发生撤销。用户态不能关 IRQ，也没有资源 pin/租约或完整异常恢复。
所以本实现保证的是**观察到危险后的停止与禁止复用**，不是“所有活跃访问都能事先发现危险”。
检查前或检查后窗口中的失效访问由 MMU 隔离，内核清理本域；不承诺每次都能走 exit(96)。

复制 payload 到另一 VA 不会更新外部原始指针；地址重用也无法区分旧/新对象。
本阶段不透明迁移，不尝试用户页错误恢复，不声称能恢复所有强制撤销。
内核将来若增加带完成记录的协议、可验证 pin/租约或受限异常处理，仍需另行设计指针语义。
