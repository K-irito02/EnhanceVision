# 队列继续处理问题修复笔记

## 概述

修复了在模式0（单任务暂停）下，暂停第一个任务后队列没有继续处理其他任务的问题。

**创建日期**: 2026-04-06
**相关 Issue**: 队列继续处理问题

---

## 变更概述

### 修改文件

| 文件路径 | 修改内容 |
|----------|----------|
| `src/controllers/ProcessingController.cpp` | 修复暂停逻辑、资源释放、引擎获取失败处理 |
| `src/core/AIEnginePool.cpp` | 修改 `availableCount` 方法，添加 `hasDrainingEngine` 方法 |
| `include/EnhanceVision/core/AIEnginePool.h` | 添加 `hasDrainingEngine` 方法声明 |

---

## 问题描述

### 现象

用户添加多个处理任务，在第一个任务（CPU子模式处理图片文件）处理时点击暂停，队列没有继续处理其他任务。

### 根本原因

问题有多个层面的原因：

1. **暂停逻辑问题**：`processNextTask` 方法在所有暂停模式下都会跳过暂停消息的所有任务，没有考虑模式0（单任务暂停）的特殊性。

2. **资源未释放**：在模式0下暂停任务时，只释放了 AI 引擎，没有释放 `ResourceManager` 中的资源，导致后续任务无法启动。

3. **引擎获取失败时资源泄漏**：当引擎池耗尽时，`startTask` 方法会将任务状态改回 Pending，但没有释放已经获取的资源。

4. **引擎状态判断问题**：引擎在 `Draining` 状态（正在释放中）时，`availableCount()` 返回 0，导致任务被错误地跳过。

---

## 技术实现细节

### 修复 1：根据暂停模式调整跳过逻辑

**文件**: `src/controllers/ProcessingController.cpp`

**修改前**：
```cpp
// 检查消息是否被暂停，跳过已暂停消息的任务
if (m_pausedMessageIds.contains(task.messageId)) {
    hasSkippedPausedTask = true;
    continue;
}
```

**修改后**：
```cpp
// 检查消息是否被暂停，根据暂停模式决定是否跳过
if (m_pausedMessageIds.contains(task.messageId)) {
    // 模式0（单任务暂停）：不跳过，只跳过状态为 Paused 的任务
    // 模式1和模式2：跳过暂停消息的所有任务
    if (pauseMode != 0) {
        hasSkippedPausedTask = true;
        continue;
    }
    // 模式0：只跳过状态为 Paused 的任务，不跳过 Pending 的任务
    if (task.status == ProcessingStatus::Paused) {
        hasSkippedPausedTask = true;
        continue;
    }
}
```

### 修复 2：在模式0下释放资源

**文件**: `src/controllers/ProcessingController.cpp`

在 `pauseMessageTasks` 方法中，模式0的处理逻辑：

```cpp
QtConcurrent::run([this, taskIdsToRelease]() {
    for (const QString& taskId : taskIdsToRelease) {
        m_aiEnginePool->release(taskId);
        qInfo() << "[ProcessingController] Mode 0: Released AI engine for task:" << taskId;
    }
    // 引擎和资源释放完成后，在主线程触发处理下一个任务
    QMetaObject::invokeMethod(this, [this, taskIdsToRelease]() {
        // 在主线程中释放资源
        for (const QString& taskId : taskIdsToRelease) {
            m_resourceManager->release(taskId);
            qInfo() << "[ProcessingController] Mode 0: Released resources for task:" << taskId;
        }
        // 然后处理下一个任务
        processNextTask();
    }, Qt::QueuedConnection);
});
```

**关键点**：资源释放在主线程中执行，避免线程安全问题。

### 修复 3：引擎获取失败时释放资源

**文件**: `src/controllers/ProcessingController.cpp`

在 `startTask` 方法中：

```cpp
AIEngine* engine = m_aiEnginePool->acquireWithBackend(taskId, requestedBackend);
if (!engine) {
    // 引擎池暂时耗尽，延迟重试而不是立即失败
    qInfo() << "[ProcessingController] AI engine pool temporarily exhausted for task:" << taskId
            << ", will retry in 500ms";
    
    // 释放资源（因为引擎获取失败）
    TaskContext ctx = m_taskCoordinator->getTaskContext(taskId);
    m_resourceManager->release(taskId);
    qInfo() << "[ProcessingController] Released resources for task (engine exhausted):" << taskId;
    
    // 将任务状态改回 Pending，稍后重试
    for (auto& t : m_tasks) {
        if (t.taskId == taskId) {
            t.status = ProcessingStatus::Pending;
            break;
        }
    }
    adjustProcessingCount(-1);
    QTimer::singleShot(500, this, &ProcessingController::processNextTask);
    return;
}
```

### 修复 4：引擎状态判断优化

**文件**: `src/core/AIEnginePool.cpp`

修改 `availableCount` 方法，将 `Draining` 状态的引擎也计入可用引擎：

```cpp
int AIEnginePool::availableCount() const
{
    int count = 0;
    for (const auto& slot : m_slots) {
        if (slot.state == EngineState::Ready || 
            slot.state == EngineState::Uninitialized) {
            if (slot.engine && !slot.engine->isProcessing()) {
                count++;
            } else if (!slot.engine) {
                // 引擎未初始化，可以创建新引擎
                count++;
            }
        } else if (slot.state == EngineState::Draining) {
            // Draining 状态的引擎即将可用，计入可用引擎
            // 这样队列可以继续处理其他任务，即使引擎还在取消处理中
            count++;
        }
    }
    return count;
}
```

添加 `hasDrainingEngine` 方法：

```cpp
bool AIEnginePool::hasDrainingEngine() const
{
    QMutexLocker locker(&m_mutex);
    for (const auto& slot : m_slots) {
        if (slot.state == EngineState::Draining) {
            return true;
        }
    }
    return false;
}
```

### 修复 5：等待引擎释放完成

**文件**: `src/controllers/ProcessingController.cpp`

在 `processNextTask` 方法中：

```cpp
if (!nextTask) {
    if (hasSkippedAITask) {
        // 检查是否有引擎正在释放中
        if (m_aiEnginePool->hasDrainingEngine()) {
            // 引擎正在释放中，等待 engineReady 信号，不立即重试
            qInfo() << "[ProcessingController] Engine is draining, waiting for engineReady signal";
        } else {
            // 没有引擎正在释放中，稍后重试
            QTimer::singleShot(200, this, &ProcessingController::processNextTask);
        }
    }
    return;
}

if (!tryStartTask(*nextTask)) {
    // 如果启动失败，检查是否有引擎正在释放中
    if (m_aiEnginePool->hasDrainingEngine()) {
        // 引擎正在释放中，等待 engineReady 信号，不立即重试
        qInfo() << "[ProcessingController] Engine is draining, waiting for engineReady signal";
    } else {
        // 没有引擎正在释放中，稍后重试
        QTimer::singleShot(100, this, &ProcessingController::processNextTask);
    }
}
```

---

## 遇到的问题及解决方案

### 问题 1：暂停后队列没有继续处理

**现象**：暂停第一个任务后，第二个任务一直显示"等待处理"状态。

**原因**：`processNextTask` 方法在所有暂停模式下都会跳过暂停消息的所有任务。

**解决方案**：根据暂停模式调整跳过逻辑，模式0下只跳过状态为 `Paused` 的任务。

### 问题 2：资源检查失败

**现象**：日志显示 `Resource check failed (canStartNewTask)`。

**原因**：在模式0下暂停任务时，只释放了 AI 引擎，没有释放 `ResourceManager` 中的资源。

**解决方案**：在模式0下暂停任务时，同时释放 AI 引擎和资源。

### 问题 3：资源泄漏

**现象**：`activeTaskCount` 变成 1，导致后续任务无法启动。

**原因**：引擎获取失败时，`startTask` 方法没有释放已经获取的资源。

**解决方案**：在引擎获取失败时，释放已经获取的资源。

### 问题 4：引擎状态判断错误

**现象**：`availableCount` 返回 0，即使引擎正在释放中。

**原因**：`Draining` 状态的引擎没有被计入可用引擎。

**解决方案**：修改 `availableCount` 方法，将 `Draining` 状态的引擎也计入可用引擎。

---

## 测试验证

| 场景 | 预期结果 | 实际结果 |
|------|----------|----------|
| 模式0：暂停第一个任务 | 队列继续处理第二个任务 | ✅ 通过 |
| 模式0：暂停后恢复 | 任务恢复处理 | ✅ 通过 |
| 引擎释放中启动新任务 | 等待引擎释放完成后启动 | ✅ 通过 |

---

## 后续工作

- [ ] 考虑优化引擎取消处理的速度，减少等待时间
- [ ] 考虑增加引擎池大小，支持并行处理多个任务
