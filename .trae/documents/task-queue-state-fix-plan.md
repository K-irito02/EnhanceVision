# 任务处理系统状态异常修复计划

## 问题概述

当用户通过"批量操作"功能删除或清空所有"正在处理"和"等待处理"状态的任务会话后，重新提交新任务时，消息卡片错误地显示为"等待处理"状态，而非正常处理状态。

## 根本原因分析

### 1. 核心问题：`m_currentProcessingCount` 状态不一致

**位置**: `ProcessingController.cpp`

**问题描述**:
- `processNextTask()` 方法依赖 `m_currentProcessingCount` 来判断是否可以启动新任务
- 当 `m_currentProcessingCount >= 1` 时，队列会拒绝启动新任务
- 在批量删除/清空操作后，`m_currentProcessingCount` 没有被正确重置为 0

**关键代码路径分析**:

```cpp
// ProcessingController.cpp:415-424
void ProcessingController::processNextTask()
{
    if (m_queueStatus != QueueStatus::Running) {
        return;
    }

    // 线性任务队列：一次只处理一个任务
    if (m_currentProcessingCount >= 1) {
        return;  // <-- 如果 m_currentProcessingCount > 0，新任务无法启动
    }
    // ...
}
```

### 2. `cancelAllTasks()` 方法缺陷

**位置**: `ProcessingController.cpp:246-267`

**问题**:
- 对于 `ProcessingStatus::Processing` 状态的任务调用 `gracefulCancel()`
- `gracefulCancel()` 只有在特定条件下才会减少 `m_currentProcessingCount`
- 方法结束时没有验证和重置 `m_currentProcessingCount`

### 3. `forceCancelAllTasks()` 方法缺陷

**位置**: `ProcessingController.cpp:269-298`

**问题**:
- 直接清空任务列表，但没有重置 `m_currentProcessingCount`
- 可能导致队列永久卡住

### 4. `gracefulCancel()` 状态处理不完整

**位置**: `ProcessingController.cpp:1918-1961`

**问题**:
- 只有当 `targetTask->status == ProcessingStatus::Processing` 时才减少 `m_currentProcessingCount`
- 如果任务状态已经改变（如 Cancelling），计数器不会被减少

### 5. 消息状态同步延迟

**位置**: `ProcessingController.cpp:1623-1695`

**问题**:
- `syncMessageStatus()` 有防抖机制（300ms）
- 在快速批量操作时，状态同步可能被跳过

## 修复方案

### 阶段一：核心状态管理修复

#### 1.1 修复 `cancelAllTasks()` 方法

**文件**: `src/controllers/ProcessingController.cpp`

**修改内容**:
- 在方法结束时添加 `m_currentProcessingCount` 验证和重置逻辑
- 确保所有正在处理的任务都被正确取消

```cpp
void ProcessingController::cancelAllTasks()
{
    int processingCount = 0;
    
    for (int i = m_tasks.size() - 1; i >= 0; --i) {
        const QString taskId = m_tasks[i].taskId;

        if (m_tasks[i].status == ProcessingStatus::Pending) {
            m_tasks[i].status = ProcessingStatus::Cancelled;
            cleanupTask(taskId);
            emit taskCancelled(taskId);
            m_tasks.removeAt(i);
            continue;
        }

        if (m_tasks[i].status == ProcessingStatus::Processing) {
            gracefulCancel(taskId);
            processingCount++;
        }
    }

    // 验证并重置处理计数
    if (processingCount == 0 && m_currentProcessingCount > 0) {
        qWarning() << "[ProcessingController] cancelAllTasks: resetting stale processing count"
                   << "current:" << m_currentProcessingCount;
        m_currentProcessingCount = 0;
        emit currentProcessingCountChanged();
    }

    updateQueuePositions();
    syncModelTasks();
    emit queueSizeChanged();
}
```

#### 1.2 修复 `forceCancelAllTasks()` 方法

**文件**: `src/controllers/ProcessingController.cpp`

**修改内容**:
- 添加 `m_currentProcessingCount` 重置逻辑

```cpp
void ProcessingController::forceCancelAllTasks()
{
    qWarning() << "[ProcessingController] Force cancelling all tasks";

    // 强制取消 AI 推理
    if (m_aiEngine) {
        m_aiEngine->forceCancel();
    }

    // 清空线程池队列
    if (m_threadPool) {
        m_threadPool->clear();
    }

    // 取消所有任务
    for (int i = m_tasks.size() - 1; i >= 0; --i) {
        const QString taskId = m_tasks[i].taskId;

        m_tasks[i].status = ProcessingStatus::Cancelled;
        cleanupTask(taskId);
        emit taskCancelled(taskId);
    }
    m_tasks.clear();

    // 重置处理计数
    m_currentProcessingCount = 0;
    emit currentProcessingCountChanged();

    updateQueuePositions();
    syncModelTasks();
    emit queueSizeChanged();

    qWarning() << "[ProcessingController] All tasks force cancelled";
}
```

#### 1.3 改进 `gracefulCancel()` 方法

**文件**: `src/controllers/ProcessingController.cpp`

**修改内容**:
- 确保无论任务状态如何，都能正确处理 `m_currentProcessingCount`

```cpp
void ProcessingController::gracefulCancel(const QString& taskId, int timeoutMs)
{
    Q_UNUSED(timeoutMs);

    m_taskCoordinator->requestCancellation(taskId);

    QueueTask* targetTask = getTask(taskId);
    bool wasProcessing = (targetTask && targetTask->status == ProcessingStatus::Processing);

    if (targetTask && targetTask->status == ProcessingStatus::Processing) {
        // ... 现有的取消逻辑 ...
    }

    cleanupTask(taskId);

    // 确保处理计数正确
    // 如果任务曾经是 Processing 状态，确保计数器被减少
    if (wasProcessing && m_currentProcessingCount > 0) {
        m_currentProcessingCount--;
        emit currentProcessingCountChanged();
        qInfo() << "[ProcessingController] gracefulCancel: decremented processing count"
                << "taskId:" << taskId
                << "new count:" << m_currentProcessingCount;
    }
}
```

### 阶段二：队列状态验证机制

#### 2.1 添加队列状态验证方法

**文件**: `include/EnhanceVision/controllers/ProcessingController.h`

**新增方法声明**:
```cpp
private:
    void validateQueueState();
    int countActualProcessingTasks() const;
```

**文件**: `src/controllers/ProcessingController.cpp`

**新增方法实现**:
```cpp
int ProcessingController::countActualProcessingTasks() const
{
    int count = 0;
    for (const auto& task : m_tasks) {
        if (task.status == ProcessingStatus::Processing) {
            count++;
        }
    }
    return count;
}

void ProcessingController::validateQueueState()
{
    const int actualCount = countActualProcessingTasks();
    
    if (m_currentProcessingCount != actualCount) {
        qWarning() << "[ProcessingController] Queue state mismatch detected"
                   << "recorded:" << m_currentProcessingCount
                   << "actual:" << actualCount
                   << "fixing...";
        
        m_currentProcessingCount = actualCount;
        emit currentProcessingCountChanged();
    }
}
```

#### 2.2 在关键位置调用验证方法

**修改位置**:
- `processNextTask()` 开始处
- `addTask()` 方法中
- `cancelMessageTasks()` 结束处
- `cancelSessionTasks()` 结束处

### 阶段三：消息卡片级别删除功能修复

#### 3.1 修复 `MessageModel::removeMessage()`

**文件**: `src/models/MessageModel.cpp`

**修改内容**:
- 确保删除消息时正确取消相关任务
- 添加任务取消完成的确认机制

```cpp
bool MessageModel::removeMessage(const QString &messageId)
{
    int index = findMessageIndex(messageId);
    if (index < 0) {
        emit errorOccurred(tr("消息不存在: %1").arg(messageId));
        return false;
    }

    emit messageRemoving(messageId);

    if (m_processingController) {
        m_processingController->cancelMessageTasks(messageId);
        // 验证队列状态
        // cancelMessageTasks 内部会调用 validateQueueState
    }

    beginRemoveRows(QModelIndex(), index, index);
    m_messages.removeAt(index);
    endRemoveRows();

    m_messageFileStatsCache.remove(messageId);
    emit countChanged();
    emit messageRemoved(messageId);
    return true;
}
```

#### 3.2 修复 `MessageModel::removeSelectedMessages()`

**文件**: `src/models/MessageModel.cpp`

**修改内容**:
- 批量删除时确保所有任务都被正确取消

```cpp
int MessageModel::removeSelectedMessages()
{
    int deletedCount = 0;
    QList<QString> toDelete;

    for (const Message &message : m_messages) {
        if (message.isSelected) {
            toDelete.append(message.id);
        }
    }

    if (m_processingController) {
        for (const QString& messageId : toDelete) {
            emit messageRemoving(messageId);
            m_processingController->cancelMessageTasks(messageId);
        }
    }

    // 从后向前删除，避免索引变化问题
    for (auto it = toDelete.rbegin(); it != toDelete.rend(); ++it) {
        int index = findMessageIndex(*it);
        if (index >= 0) {
            beginRemoveRows(QModelIndex(), index, index);
            m_messages.removeAt(index);
            endRemoveRows();
            m_messageFileStatsCache.remove(*it);
            emit messageRemoved(*it);
            deletedCount++;
        }
    }

    if (deletedCount > 0) {
        emit countChanged();
    }

    return deletedCount;
}
```

### 阶段四：单个多媒体文件删除功能修复

#### 4.1 修复 `MessageModel::removeMediaFile()`

**文件**: `src/models/MessageModel.cpp`

**修改内容**:
- 确保删除单个文件时正确取消相关任务

```cpp
bool MessageModel::removeMediaFile(const QString &messageId, int fileIndex)
{
    int msgIdx = findMessageIndex(messageId);
    if (msgIdx < 0) {
        emit errorOccurred(tr("消息不存在: %1").arg(messageId));
        return false;
    }

    Message &message = m_messages[msgIdx];
    if (fileIndex < 0 || fileIndex >= message.mediaFiles.size()) {
        emit errorOccurred(tr("文件索引无效: %1").arg(fileIndex));
        return false;
    }

    const QString removedFileId = message.mediaFiles[fileIndex].id;
    if (m_processingController && !removedFileId.isEmpty()) {
        m_processingController->cancelMessageFileTasks(messageId, removedFileId);
    }

    message.mediaFiles.removeAt(fileIndex);
    emit messageMediaFilesReloaded(messageId);
    emitMessageFileStatsChanged(messageId, message);
    
    if (message.mediaFiles.isEmpty()) {
        beginRemoveRows(QModelIndex(), msgIdx, msgIdx);
        m_messages.removeAt(msgIdx);
        endRemoveRows();
        m_messageFileStatsCache.remove(messageId);
        emit countChanged();
        emit messageRemoved(messageId);
    } else {
        QModelIndex modelIndex = createIndex(msgIdx, 0);
        emit dataChanged(modelIndex, modelIndex, {MediaFilesRole});
        emit mediaFileRemoved(messageId, fileIndex);
    }
    
    return true;
}
```

### 阶段五：资源释放与线程安全

#### 5.1 改进 `cleanupTask()` 方法

**文件**: `src/controllers/ProcessingController.cpp`

**修改内容**:
- 添加资源释放的完整性检查
- 添加线程安全保护

```cpp
void ProcessingController::cleanupTask(const QString& taskId)
{
    if (taskId.isEmpty()) {
        return;
    }

    cancelVideoProcessing(taskId);
    
    m_resourceManager->release(taskId);
    m_taskCoordinator->unregisterTask(taskId);
    m_pendingExports.remove(taskId);
    m_taskContexts.remove(taskId);
    m_lastReportedTaskProgress.remove(taskId);
    m_lastTaskProgressUpdateMs.remove(taskId);
    m_taskMessages.remove(taskId);
    m_pendingModelLoadTaskIds.remove(taskId);

    disconnectAiEngineForTask(taskId);
    m_aiEnginePool->release(taskId);
    
    qInfo() << "[ProcessingController] cleanupTask completed:" << taskId;
}
```

#### 5.2 添加 `clear()` 方法到 ProcessingController

**文件**: `include/EnhanceVision/controllers/ProcessingController.h`

**新增方法声明**:
```cpp
public:
    Q_INVOKABLE void clearAllTasks();
```

**文件**: `src/controllers/ProcessingController.cpp`

**新增方法实现**:
```cpp
void ProcessingController::clearAllTasks()
{
    QMutexLocker locker(&m_mutex);  // 如果需要线程安全
    
    forceCancelAllTasks();
    
    // 清理所有相关状态
    m_taskToMessage.clear();
    m_pendingExports.clear();
    m_taskContexts.clear();
    m_lastReportedTaskProgress.clear();
    m_lastTaskProgressUpdateMs.clear();
    m_lastMessageProgressSyncMs.clear();
    m_lastMessageStatusSyncMs.clear();
    m_lastSessionMemorySyncMs.clear();
    m_pendingMemorySyncMessageIds.clear();
    m_pendingModelLoadTaskIds.clear();
    
    m_currentProcessingCount = 0;
    emit currentProcessingCountChanged();
    
    qInfo() << "[ProcessingController] All tasks and states cleared";
}
```

### 阶段六：日志记录与监控

#### 6.1 添加详细日志

**修改位置**:
- `addTask()` - 记录任务添加
- `processNextTask()` - 记录任务启动决策
- `cancelAllTasks()` - 记录取消操作
- `gracefulCancel()` - 记录取消详情
- `validateQueueState()` - 记录状态验证结果

#### 6.2 添加状态监控信号

**文件**: `include/EnhanceVision/controllers/ProcessingController.h`

**新增信号**:
```cpp
signals:
    void queueStateValidated(bool consistent, int recordedCount, int actualCount);
```

## 测试计划

### 测试用例 1：批量删除后新任务提交

**步骤**:
1. 提交多个任务（包含正在处理和等待处理的任务）
2. 使用批量操作删除所有任务
3. 提交新任务
4. 验证新任务状态正确显示为"处理中"

**预期结果**:
- 新任务立即进入"处理中"状态
- `m_currentProcessingCount` 正确为 1

### 测试用例 2：清空所有消息后新任务提交

**步骤**:
1. 提交多个任务
2. 清空所有消息
3. 提交新任务
4. 验证新任务状态正确

**预期结果**:
- 新任务立即进入"处理中"状态
- 队列状态正确重置

### 测试用例 3：单个文件删除后队列继续

**步骤**:
1. 提交包含多个文件的任务
2. 删除正在处理的单个文件
3. 验证队列继续处理下一个文件

**预期结果**:
- 队列正确处理剩余文件
- 状态计数正确

### 测试用例 4：高并发任务提交

**步骤**:
1. 快速连续提交多个任务
2. 在处理过程中批量删除
3. 再次提交新任务

**预期结果**:
- 状态一致性保持
- 无死锁或卡顿

### 测试用例 5：会话切换场景

**步骤**:
1. 在会话 A 中提交任务
2. 切换到会话 B
3. 在会话 B 中提交任务
4. 切换回会话 A 并删除任务

**预期结果**:
- 跨会话状态正确
- 任务取消正确传播

## 实现优先级

1. **高优先级**（核心修复）:
   - 修复 `cancelAllTasks()` 
   - 修复 `forceCancelAllTasks()`
   - 改进 `gracefulCancel()`
   - 添加 `validateQueueState()`

2. **中优先级**（功能完善）:
   - 修复消息卡片级别删除
   - 修复单个文件删除
   - 添加 `clearAllTasks()` 方法

3. **低优先级**（增强功能）:
   - 添加详细日志
   - 添加状态监控信号

## 风险评估

### 潜在风险

1. **线程安全问题**:
   - 多个方法可能被不同线程调用
   - 需要确保状态变更的原子性

2. **信号循环**:
   - 状态变更可能触发信号
   - 需要避免无限循环

3. **性能影响**:
   - 频繁的状态验证可能影响性能
   - 需要在关键位置进行验证

### 缓解措施

1. 使用互斥锁保护关键状态
2. 添加状态变更的防抖机制
3. 限制验证频率

## 文件修改清单

| 文件路径 | 修改类型 | 说明 |
|---------|---------|------|
| `src/controllers/ProcessingController.cpp` | 修改 | 核心修复 |
| `include/EnhanceVision/controllers/ProcessingController.h` | 修改 | 新增方法声明 |
| `src/models/MessageModel.cpp` | 修改 | 删除功能修复 |
| `include/EnhanceVision/models/MessageModel.h` | 检查 | 确认接口完整 |
| `src/core/TaskCoordinator.cpp` | 检查 | 确认状态同步 |

## 验收标准

1. 批量删除/清空操作后，新任务能正确进入"处理中"状态
2. 任务队列处理顺序符合 FIFO 原则
3. 系统在高并发场景下保持状态一致性
4. 无内存泄漏和资源残留
5. 日志记录完整，便于问题追踪
