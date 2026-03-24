# 03 C++ 后端规范

## 命名与结构

- 类名 PascalCase，函数/变量 camelCase，成员 `m_` 前缀
- 头文件与实现文件保持一一对应
- 新增类必须明确归属 `controllers/core/models/providers/services/utils`

## Qt 集成

- `Q_PROPERTY` 必须有完整 `READ/WRITE/NOTIFY`
- 跨 QML 暴露接口优先简单类型与 `QVariant`
- 信号命名统一 `xxxChanged` / `xxxFinished`

## 内存与资源

- 优先智能指针与 RAII
- `QObject` 明确父子关系
- 禁止裸 `new/delete` 式无归属对象

## 错误与日志

- 错误统一可观测（信号或返回值）
- 关键路径必须有日志上下文（模块名、动作、结果）
- 禁止吞错和静默失败
