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
| ResourceManager | 资源配额管理 |
| TaskCoordinator | 任务协调与调度 |
| VideoProcessor | 视频处理管道 |
| ImageProcessor | 图像处理管道 |

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
