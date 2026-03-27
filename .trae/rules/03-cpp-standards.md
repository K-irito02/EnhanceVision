---
alwaysApply: false
globs: ['**/*.cpp', '**/*.h', '**/*.hpp']
description: 'C++代码规范 - 命名、接口设计、内存安全、错误处理'
trigger: glob
---
# C++ 代码规范

## 命名规范

| 类型 | 风格 | 示例 |
|------|------|------|
| 类/枚举 | PascalCase | `AIEngine` |
| 函数/变量 | camelCase | `processImage()` |
| 成员变量 | `m_`前缀 | `m_modelPath` |
| 常量 | `k`前缀 | `kMaxThreads` |
| 信号 | `xxxChanged` | `progressChanged()` |

## Qt 接口规范

- `Q_PROPERTY` 必须带 `NOTIFY`（可观察属性）
- `Q_INVOKABLE` 仅暴露轻量同步或异步入口
- 阻塞操作不得直接暴露给 QML 主线程调用

## 类型与接口设计

- 公共接口优先清晰、稳定、最小化
- 复杂返回使用 `QVariant`/结构体，避免参数爆炸
- 一次只做一件事，函数职责单一

## 内存与资源

- 优先 RAII、智能指针
- `QObject` 明确父子关系
- 禁止无归属裸指针生命周期

## 错误处理

- 错误必须可观测（返回值/信号/日志）
- 禁止吞错与静默失败
- GPU OOM 必须自动恢复：清理内存 → 重试 → 降级分块
- 进度更新只允许前进，避免 UI 倒退体验

## 多线程与并发

- **线程安全**：避免在多线程函数中使用静态变量
- **资源隔离**：每个线程使用独立的资源副本
- **QtConcurrent**：异步任务使用 `QtConcurrent::run`，通过信号槽传递结果
- **FFmpeg线程**：视频处理中的 FFmpeg 上下文在线程内管理，不跨线程共享
- **RAII资源管理**：使用 cleanup lambda 确保异常安全

## 本文件边界

- 仅定义 C++ 编码规范
- 线程与性能细则见 `07-performance.md`
