---
alwaysApply: false
globs: ['**/src/**/*.h', '**/src/**/*.hpp', '**/src/**/*.cpp', '**/qml/**/*.qml']
description: '架构设计 - 分层职责、模块边界、数据流'
trigger: glob
---
# 架构设计

## 分层职责
  - QML UI 层（负责展示、交互、动画等）
  - C++ 业务层（负责业务逻辑、状态管理、任务调度等）
  - C++ 核心层（负责 AI 模型推理、图像处理、视频处理等）

## 模块职责

| 模块 | 职责 |
|------|------|
| Controller | 状态暴露 + 业务编排 + 任务管理 |
| Model | 列表数据（QAbstractListModel） |
| Provider | 图像桥接（QQuickImageProvider） |
| Service | 跨模块复用业务 |
| Core | 重计算能力与引擎 |

## 核心组件

| 组件 | 职责 |
|------|------|
| AIEngine | AI 模型加载与推理 |
| AIEnginePool | AI 引擎池管理（并发控制） |
| ProcessingController | 处理任务编排 |
| TaskRecoveryController | 启动恢复编排（快照/待恢复/恢复决策） |
| ResourceManager | 资源配额管理 |
| TaskCoordinator | 任务协调与调度 |
| VideoProcessor | 视频处理管道 |
| ImageProcessor | 图像处理管道（QtConcurrent 并行行处理） |
| TaskTimeEstimator | 任务时间预测与动态修正 |
| SupportedFormats | 统一格式注册表（唯一真相源） |

## 启动恢复约束

当应用退出时，如果仍存在未完成任务（`处理中 / 等待中 / 暂停中`），下次启动必须走统一恢复流程：

1. 未完成任务先统一映射为 `待恢复`（`ProcessingStatus::Recoverable`），避免启动后立即自动跑导致语义错乱
2. 用户必须在启动恢复弹窗中二选一：恢复关闭前状态，或全部标记为失败
3. 在恢复决策完成前不允许切换任务控制模式；决策完成后，只要仍有未完成任务也不允许切换

## 数据流规范

| 类型 | 方式 |
|------|------|
| 状态 | `Q_PROPERTY + NOTIFY` |
| 命令 | `Q_INVOKABLE` |
| 异步结果 | 信号回传 |

## 架构红线

1. **禁止 QML 承载复杂业务流程**
2. **禁止 Controller 直接操作 QML 对象**
3. **禁止跨层循环依赖**
4. **禁止在工作线程操作 UI 对象**
5. **禁止在构造函数中阻塞等待异步初始化**
