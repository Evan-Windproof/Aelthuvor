# Phase 2：Preemption & Revocation Integration

Phase 1 基准为 `b10d337d254b18e98204990203a77a215c82c568`，保持其 first-fit/best-fit、
页面包装和应用入口。Phase 2 只修改 Aelthuvor。内核参考为 Liberums
`cdd83e7ff7d6ca55e05f3433945e0bbee2c4ca9c`（远程 main）；它较 Phase 6 实现提交
`85a493466afe2038ca22a9870f6f9ce37dd7f3a8` 只增加审查文档、证据和测试。

内核提供硬件隔离、完整 generation 验证、CPU 时间片、撤销期限与最终清零回收。
LibOS 提供 VA 选择、块表、策略、空 Arena 主动确认和活跃内存的失败决策。策略不进入 Ring 0。

本阶段新增独立 revoke 模块、公开的同步 heap 检查点和页面终态。单 CPU、每域单线程下，
跨域抢占只暂停当前指令流，不产生同域并行写入，所以不添加分配器锁。
新 Preemptive ELF 要求 PREEMPTION/REVOCATION；Cooperative 保持主动 yield。

活跃 Arena 撤销采取失败停止，不等待应用异步回调，不复制到新地址后假称旧指针仍有效。
本阶段交付了独立测试与真实抢占集成；撤销客户端的真实外部端到端验收仍是部分验收。
详细的通过和未执行项目见 phase2-validation.md。到此停止，不进入 Phase 3。
