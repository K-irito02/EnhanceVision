---
alwaysApply: false
globs: ['**/src/**/*.h', '**/src/**/*.hpp', '**/src/**/*.cpp', '**/qml/**/*.qml']
description: '架构设计 - 分层职责、模块边界、数据流与跨层约束'
trigger: glob
---
# 架构设计

## 分层职责

```
┌─────────────────────────────────────┐
│         QML UI 层                   │
│  展示、交互、动画                    │
└─────────────────────────────────────┘
              ↕ Q_PROPERTY / 信号槽
┌─────────────────────────────────────┐
│         C++ 业务层                  │
│  Controller / Model / Provider      │
│  Service / Utils                    │
└─────────────────────────────────────┘
              ↕ 任务调度
┌─────────────────────────────────────┐
│         C++ 核心层                  │
│  AI引擎 / 图像处理 / 视频处理       │
│  并发调度 / 资源管理                │
└─────────────────────────────────────┘
```

## 模块职责

| 模块 | 职责 |
|------|------|
| **Controller** | 状态暴露 + 业务编排 + 并发任务管理 |
| **Model** | 列表数据与角色字段（QAbstractListModel） |
| **Provider** | 图像桥接（预览/缩略图，QQuickImageProvider） |
| **Service** | 跨模块复用业务（如 ImageExportService） |
| **Core** | 重计算能力与引擎 + 并发调度系统 |

## 核心组件

| 组件 | 文件 | 职责 |
|------|------|------|
| AIEngine | `core/AIEngine.cpp` | AI模型加载与推理 |
| AIEnginePool | `core/AIEnginePool.cpp` | AI引擎池管理（2个并发） |
| ProcessingEngine | `core/ProcessingEngine.cpp` | 图像/视频处理核心引擎 |
| TaskCoordinator | `core/TaskCoordinator.cpp` | 任务协调与调度 |
| TaskStateManager | `core/TaskStateManager.cpp` | 任务状态追踪与管理 |
| ResourceManager | `core/ResourceManager.cpp` | 资源生命周期管理 |
| FrameCache | `core/FrameCache.cpp` | 视频帧缓存管理 |
| ModelRegistry | `core/ModelRegistry.cpp` | AI模型注册与发现 |
| ProgressManager | `core/ProgressManager.cpp` | 进度聚合与分发 |
| ProgressReporter | `core/ProgressReporter.cpp` | 进度报告接口 |
| LifecycleSupervisor | `core/LifecycleSupervisor.cpp` | 组件生命周期监督 |
| ImageProcessor | `core/ImageProcessor.cpp` | 图像处理管道 |
| VideoProcessor | `core/VideoProcessor.cpp` | 视频处理管道 |
| CancellableTaskToken | `core/CancellableTaskToken.cpp` | 任务取消令牌 |

## 视频处理子模块

| 组件 | 文件 | 职责 |
|------|------|------|
| AIVideoProcessor | `core/video/AIVideoProcessor.cpp` | AI视频增强处理 |
| VideoFrameBuffer | `core/video/VideoFrameBuffer.cpp` | 视频帧缓冲管理 |
| VideoResourceGuard | `core/video/VideoResourceGuard.cpp` | 视频资源守卫 |
| VideoSizeAdapter | `core/video/VideoSizeAdapter.cpp` | 视频尺寸适配 |
| VideoFormatConverter | `core/video/VideoFormatConverter.cpp` | 视频格式转换 |
| VideoCompatibilityAnalyzer | `core/video/VideoCompatibilityAnalyzer.cpp` | 视频兼容性分析 |
| VideoProcessorFactory | `core/video/VideoProcessorFactory.cpp` | 视频处理器工厂 |

## 数据流规范

- **状态**：`Q_PROPERTY + NOTIFY`
- **命令**：`Q_INVOKABLE`
- **异步结果**：信号回传
- **UI通过绑定消费状态**，避免命令式同步

## 架构红线

- ❌ 禁止 QML 承载复杂业务流程
- ❌ 禁止 Controller 直接操作 QML 对象
- ❌ 禁止跨层循环依赖
- ❌ 禁止在工作线程操作 UI 对象

## 本文件边界

- 仅定义"架构与边界"
- 具体编码规范见 `03`、`04`
- 性能与线程细则见 `07`
