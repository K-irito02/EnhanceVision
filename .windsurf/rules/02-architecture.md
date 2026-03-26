---
alwaysApply: false
globs: ['**/src/**/*.h', '**/src/**/*.hpp', '**/src/**/*.cpp', '**/qml/**/*.qml']
description: '架构设计 - 分层职责、模块边界、数据流与跨层约束'
trigger: glob
---
# 架构设计

## 分层职责

1. **QML UI 层**：展示、交互、动画
2. **C++ 业务层**：Controller / Model / Provider / Service
3. **C++ 核心层**：AI、图像、视频、缓存、任务调度

## 模块职责

- `Controller`：状态暴露 + 业务编排 + 并发任务管理
- `Model`：列表数据与角色字段
- `Provider`：图像桥接（预览/缩略图）
- `Service`：跨模块复用业务
- `Core`：重计算能力与引擎 + 并发调度系统

## 并发架构组件

- `ConcurrencyManager`：统一并发管理门面
- `PriorityTaskQueue`：多级优先级任务队列
- `PriorityAdjuster`：优先级动态调整器
- `DeadlockDetector`：死锁检测与恢复
- `TaskTimeoutWatchdog`：超时监控器
- `TaskRetryPolicy`：重试策略管理
- `AIEnginePool`：AI引擎池管理
- `ConcurrencyMonitor`：实时监控与统计

## 数据流规范

- 状态：`Q_PROPERTY + NOTIFY`
- 命令：`Q_INVOKABLE`
- 异步结果：信号回传
- UI 通过绑定消费状态，避免命令式同步

## 架构红线

- 禁止 QML 承载复杂业务流程
- 禁止 Controller 直接操作 QML 对象
- 禁止跨层循环依赖
- 禁止在工作线程操作 UI 对象

## 本文件边界

- 仅定义“架构与边界”
- 具体编码规范见 `03`、`04`
- 性能与线程细则见 `07`
