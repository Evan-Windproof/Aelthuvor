# ABI 来源与兼容规则

当前公开 SDK 固定至 Liberums ABI 1.2。精确来源与 SHA256 在 sdk/manifest.json；
规则、执行 Profile 和旧二进制重建详见 [Phase 2 ABI](phase2-abi.md)。

必须区分 SDK 字节完整性与运行时兼容：

1. check-sdk 校验受控文件集合、每个 SHA256、major/minor。更新 SDK 时显式比对来源 Git 对象。
2. ELF note 检查 major 相等、minor 不旧、required features 完整；不兼容时加载 -93。
3. 运行时 ABI_QUERY/RESOURCE_LIMITS 再检查；运行不兼容时 exit(93)。
4. 从 Phase 1 原始源码/SDK 重建的 ABI 1.1 ELF 需在新版 cooperative 内核实际运行，不能以新 ELF 替代。

int 0x80：EAX=号/状态，EBX/ECX/EDX/ESI 为标量参数；除 EAX 与指定 EDX 输出外保持寄存器。
extra 是用户本地指针，内核不解引用它。旧 syscall 0..4、16..23 与权限位没有修改。
新增撤销协议 24..26 的 request/generation 为完整 uint32_t；状态与高位数据不能混淆。

旧 tests/integration.py 的工作目录头相等门禁和内核历史 libos.py 固定 0x10001 的断言不能证明兼容。
新版保留来源/哈希和核心硬件断言，替换为独立的 major/minor/features 判断，绝不修改内核测试。
详细替代覆盖和真实结果见 phase2-validation.md。

## Phase 3 多来源 SDK

四个核心 ABI 1.2 头及其 Phase 6 来源不变；新增 vga.h/keyboard.h 在 manifest.extensions.devices
单独固定到 Phase 7.3 完整提交及各自 SHA256。校验器检查所有来源的文件并拒绝重复声明。
运行时版本仍为 1.2，设备没有新 feature bit；必须 INFO 探测并 ACQUIRE 验证 grant。
旧内核 ENOSYS、无权限 EPERM、过期 generation ESTALE 均作为错误处理。
详见 [设备 ABI](phase3-device-abi.md)。历史 ABI 1.1 和双 Profile 回归继续保留。
