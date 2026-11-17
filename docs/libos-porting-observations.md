# 移植观察与 Phase 10 建议

这些是实际移植的实验依据，不是未来教程的正式发布。

- 公开 SDK 足以独立构建和运行两种策略的 LibOS；同一内核不需要重新链接用户对象，也不识别 ael_*。
- 入口、ELF note、寄存器 wrapper、资源回滚必须由 LibOS 自己保证；只删 COOPERATIVE 位不是抢占实现。
- 同一用户 VA 可映射不同函数/数据。GDB 的 Python breakpoint callback 需要按当前域身份过滤，再检查 CR3/栈。
- ABI 来源校验不能要求历史 SDK 字节与当前头相同；使用固定 Git 对象检查来源，另做运行兼容检查。
- 普通应用模板与独立 LibOS 模板不是一回事。前者消费 App API 和库，后者直接实现 Kernel ABI 客户端。
- 静态 archive 不应包含具体应用 main。显式 entry adapter 与链接顺序能防止隐藏入口选择。
- 同一 OUT 切换策略/Profile 必须改变构建配置记录，否则可能悄悄保留旧 runtime。
- opaque context 减少耦合，但不能保护同域 C 内存；内核仍独立检查能力，应用不能因此获得更多权限。

建议未来 Liberums Phase 10 在自身仓库接受：中立接口笔记、固定对象池参考、构建/链接说明、
兼容矩阵、错误对抗 fixture、QEMU/GDB 回收证据，以及“应用 vs LibOS”的两条教学路线。
正式纳入时应重新固定来源、审查许可证、在自身 CI 复现，而不是复制一份报告就宣称已支持所有 LibOS。

仍待后续可靠性工作：外部普通活跃 RAM 撤销的完整端到端触发、完成记录或受限恢复接口的设计；
当前不透明迁移，不掩盖 TOCTOU。GTK 实体输入仍需本地人工检查。
本阶段没有修改 Liberums，也不预先开发其 Phase 9/10 内核架构。
