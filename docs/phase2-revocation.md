# Visible Revocation 客户端

公开 ABI 1.2 的队列每域最多两个活动事件。POLL 非消费式，返回最早活动请求。
请求 ID 和 generation 始终保存为 uint32_t；最高位不作为错误码解释。
状态仅从 EAX 读取；EDX 即使最高位为 1 仍是有效输出。

| 调用 | 输入 | 输出/含义 |
| --- | --- | --- |
| POLL 24 | 无 | EAX=0、EDX=request；无事件 ENOENT |
| INFO 25 | EBX=request，ECX=0/1/2/3 | EDX=index/generation/reason/deadline |
| ACK 26 | EBX=index，ECX=generation，EDX=request | EAX=状态；成功即由内核撤销映射并清零回收 |

`ael_revoke_poll` 收集完整快照，任意 INFO 失败都不发布部分事件。
ENOENT 只在 POLL 时是正常无事件；INFO 的 ENOENT 归为 ESTALE。INFO 与 ACK 都可能在期间过期。
不根据 deadline 猜测仍有时间：用户态没有可靠时钟/完成通知，直接依赖 ACK 状态。

service 首先 QUERY 所有本地 owned 句柄，发现消失的强制完成。
收到事件时匹配 index + 完整 generation，不匹配即 fail closed，绝不标记另一个页。
元数据位于静态 image/BSS 和栈，不位于会撤销的 payload。

- 空 Arena 或未交给 Arena 的页面：置 PENDING，ACK 成功后清空 Arena 并置 ACKED。
- 有活跃块：置 PENDING，返回 EBUSY，不 ACK。分配器入口据此 exit(96)。
- ACK 过期/错误：保留 PENDING、request 和完整句柄，sticky failure；不重发、不 release、不复用。
- 正常 release：内核取消事件，成功后本地 RELEASED。后续 POLL 无事件。
- QUERY ESTALE：置 LOST；空 Arena 可移除，活跃 Arena 保留取证元数据并失败停止。
- 未知事件、错误字段、协议错误：heap sticky failure，不更新不相关页面。

一次 service 最多处理两个事件，再用一次 POLL 确认空队列；重复最早 ID 或超过容量立即 EBUSY。
不会因 POLL 不消费而无限循环。高层不会重复 ACK；底层 raw ACK 用于协议诊断，重复调用由内核 ESTALE 拒绝。

当前选择立即失败停止活跃撤销，没有用户异步通知、等待释放或迁移策略。强制完成后事件被删除，
POLL/INFO 无法还原完整历史。QUERY 只是额外检测，不是资源租约或原子访问保证。
真正外部撤销 E2E 尚未验证；未来内核测试接口建议见 phase2-validation.md。
