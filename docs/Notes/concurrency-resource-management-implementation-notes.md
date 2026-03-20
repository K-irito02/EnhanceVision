# 多媒体文件处理系统并发与资源管理实现笔记

## 概述

实现了多媒体文件处理系统的并发控制和资源管理机制，解决了多任务并发处理、任务取消、会话/消息删除时的资源泄漏等问题。

**创建日期**: 2026-03-21
**作者**: AI Assistant

---

## 一、完成的功能

### 1. 创建的新组件

| 组件 | 文件路径 | 功能描述 |
|------|---------|---------|
| TaskCoordinator | `include/EnhanceVision/core/TaskCoordinator.h`<br>`src/core/TaskCoordinator.cpp` | 任务协调器，管理任务与会话/消息的关联关系，协调取消和清理操作 |
| ResourceManager | `include/EnhanceVision/core/ResourceManager.h`<br>`src/core/ResourceManager.cpp` | 资源管理器，监控系统资源使用，动态调整并发限制 |

### 2. 修改的文件

| 文件 | 修改内容 |
|------|---------|
| `include/EnhanceVision/models/DataTypes.h` | 添加 TaskState 枚举、TaskContext 结构、ResourceQuota 结构 |
| `include/EnhanceVision/controllers/ProcessingController.h` | 集成 TaskCoordinator 和 ResourceManager，添加取消和资源管理方法 |
| `src/controllers/ProcessingController.cpp` | 实现并发控制、资源申请释放、孤儿任务处理 |
| `include/EnhanceVision/core/ImageProcessor.h` | 添加取消令牌支持 |
| `src/core/ImageProcessor.cpp` | 实现带取消令牌的异步处理方法 |
| `include/EnhanceVision/models/MessageModel.h` | 添加 ProcessingController 引用 |
| `src/models/MessageModel.cpp` | 删除消息时先取消任务 |
| `include/EnhanceVision/controllers/SessionController.h` | 添加 ProcessingController 引用 |
| `src/controllers/SessionController.cpp` | 删除会话时先取消任务 |
| `include/EnhanceVision/models/SessionModel.h` | 添加 sessionAt 方法 |
| `src/models/SessionModel.cpp` | 实现 sessionAt 方法 |
| `CMakeLists.txt` | 添加新文件到构建系统 |

### 3. 实现的功能特性

- ✅ 任务关联追踪：维护 sessionId/messageId/taskId 的关联关系
- ✅ 优雅取消机制：通过取消令牌实现任务的优雅取消
- ✅ 资源监控：监控系统内存使用，动态调整并发限制
- ✅ 资源压力检测：内存使用超过阈值时自动暂停新任务
- ✅ 孤儿任务处理：消息/会话删除后自动清理相关任务
- ✅ 等待取消完成：提供等待任务取消完成的机制

---

## 二、遇到的问题及解决方案

### 问题 1：取消机制不完善

**现象**：`cancel()` 只是设置标志，处理线程可能继续执行，资源未释放

**原因**：ImageProcessor 和 VideoProcessor 没有提供外部取消令牌机制

**解决方案**：
- 引入 `std::shared_ptr<std::atomic<bool>>` 取消令牌
- 在处理循环的关键点检查取消状态
- 提供 `shouldContinue()` 和 `waitIfPaused()` 方法

### 问题 2：会话/消息删除时的资源泄漏

**现象**：删除消息/会话后，正在处理的任务继续运行，结果无处存放

**原因**：MessageModel 和 SessionController 直接删除数据，未通知 ProcessingController

**解决方案**：
- 在 MessageModel::removeMessage() 中先调用 ProcessingController::cancelMessageTasks()
- 在 SessionController::deleteSession() 中先调用 ProcessingController::cancelSessionTasks()
- 使用 TaskCoordinator 维护关联关系

### 问题 3：并发资源无限制

**现象**：系统可能同时处理过多任务，导致内存耗尽

**原因**：没有资源配额和并发限制机制

**解决方案**：
- 创建 ResourceManager 管理资源配额
- 任务启动前申请资源，完成后释放
- 监控系统内存使用，压力过高时暂停新任务

---

## 三、技术实现细节

### 3.1 核心数据结构

```cpp
// 任务状态枚举
enum class TaskState {
    Pending,        // 队列等待
    Preparing,      // 准备资源
    Running,        // 处理中
    Cancelling,     // 取消中
    Cancelled,      // 已取消
    Completed,      // 已完成
    Failed,         // 失败
    Orphaned        // 孤儿任务
};

// 任务上下文
struct TaskContext {
    QString taskId;
    QString messageId;
    QString sessionId;
    QString fileId;
    qint64 estimatedMemoryMB;
    TaskState state;
    std::shared_ptr<std::atomic<bool>> cancelToken;
    std::shared_ptr<std::atomic<bool>> pauseToken;
};

// 资源配额
struct ResourceQuota {
    int maxConcurrentTasks = 4;
    qint64 maxMemoryMB = 4096;
    qint64 maxGpuMemoryMB = 2048;
};
```

### 3.2 关键流程

**删除消息流程**：
```
用户删除消息 → MessageModel.removeMessage() 
→ ProcessingController.cancelMessageTasks() 
→ TaskCoordinator 触发取消令牌 
→ 等待取消完成（最多2秒） 
→ 从列表移除消息
```

**并发处理流程**：
```
发送处理请求 → 估算资源需求 → 注册到 TaskCoordinator 
→ 检查 ResourceManager.canStartNewTask() 
→ 申请资源 ResourceManager.tryAcquire() 
→ 启动任务
```

### 3.3 资源监控

ResourceManager 使用 Windows API 查询系统内存：

```cpp
#ifdef Q_OS_WIN
MEMORYSTATUSEX status;
status.dwLength = sizeof(status);
GlobalMemoryStatusEx(&status);
// status.ullTotalPhys / status.ullAvailPhys
#endif
```

---

## 四、架构图

```
┌─────────────────────────────────────────────────────────────────────────┐
│                              QML UI 层                                  │
│  ┌─────────────┐  ┌─────────────┐  ┌─────────────┐  ┌─────────────┐   │
│  │ MainPage    │  │ MessageList │  │ ControlPanel│  │ Sidebar     │   │
│  └──────┬──────┘  └──────┬──────┘  └──────┬──────┘  └──────┬──────┘   │
└─────────┼────────────────┼────────────────┼────────────────┼───────────┘
          │                │                │                │
          ▼                ▼                ▼                ▼
┌─────────────────────────────────────────────────────────────────────────┐
│                          C++ 控制器层                                   │
│  ┌──────────────────┐  ┌──────────────────┐  ┌──────────────────┐      │
│  │SessionController │  │ProcessingController│  │ FileController  │      │
│  └────────┬─────────┘  └────────┬─────────┘  └─────────────────┘      │
│           │                     │                                      │
│           ▼                     ▼                                      │
│  ┌─────────────────────────────────────────────────────────────────┐   │
│  │                    TaskCoordinator                               │   │
│  │  - 任务生命周期管理                                              │   │
│  │  - 会话-任务关联追踪                                             │   │
│  │  - 取消/清理协调                                                 │   │
│  └─────────────────────────────────────────────────────────────────┘   │
└─────────────────────────────────────────────────────────────────────────┘
          │
          ▼
┌─────────────────────────────────────────────────────────────────────────┐
│                          处理引擎层                                     │
│  ┌──────────────────┐  ┌──────────────────┐                           │
│  │ ImageProcessor   │  │ VideoProcessor   │                           │
│  │ - 支持可中断     │  │ - 支持可中断     │                           │
│  └──────────────────┘  └──────────────────┘                           │
│                                                                         │
│  ┌─────────────────────────────────────────────────────────────────┐   │
│  │                    ResourceManager                               │   │
│  │  - 内存使用监控                                                   │   │
│  │  - 并发任务数限制                                                 │   │
│  │  - 资源压力降级                                                   │   │
│  └─────────────────────────────────────────────────────────────────┘   │
└─────────────────────────────────────────────────────────────────────────┘
```

---

## 五、参考资料

- [设计方案文档](/.trae/documents/multimedia-processing-concurrency-design.md)
- [Qt Concurrent Programming](https://doc.qt.io/qt-6/qtconcurrent-index.html)
- [std::atomic Reference](https://en.cppreference.com/w/cpp/atomic/atomic)
