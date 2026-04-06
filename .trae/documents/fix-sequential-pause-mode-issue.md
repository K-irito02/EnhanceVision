# 修复计划：顺序暂停模式下第二个视频一直加载的问题

## 问题描述

用户选择了第二种模式（顺序暂停，pauseMode = 1），在处理 Shader 模式视频文件时：
- 第一个视频能处理成功
- 第二个视频一直处于加载状态
- 用户没有点击任何按钮

## 问题分析

### 根本原因

在 `processNextTask` 函数中，模式二（顺序暂停）的逻辑：

```cpp
if (pauseMode == 1) {
    for (const auto& task : m_tasks) {
        if (task.status == ProcessingStatus::Pending) {
            if (m_pausedMessageIds.contains(task.messageId)) {
                return;  // 直接返回，不处理任何任务
            }
            break;
        }
    }
}
```

当消息 ID 在 `m_pausedMessageIds` 中时，该消息的所有 Pending 任务都会被跳过。

### 问题场景

1. 用户添加两个视频文件，创建两个任务（都为 Pending 状态）
2. 第一个任务开始处理
3. 第一个任务处理完成，调用 `finalizeTask`
4. 在 `finalizeTask` 中，只有当 `allFilesSettled` 为 true 时才会从暂停集合移除消息 ID
5. 但第二个视频还在 Pending 状态，所以 `allFilesSettled` 为 false
6. 消息 ID 仍在 `m_pausedMessageIds` 中
7. `processNextTask` 被调用，检查第一个 Pending 任务（第二个视频）
8. 发现消息 ID 在暂停集合中，直接返回
9. 第二个视频永远不会被处理

### 消息 ID 进入暂停集合的可能原因

1. 会话恢复时，如果之前的会话有暂停状态
2. 用户之前暂停过该消息
3. 其他操作（如删除文件）触发了暂停状态传递

## 修复方案

### 方案一：修改 `finalizeTask` 逻辑（推荐）

在 `finalizeTask` 中，当消息中有文件处理完成时：
- 如果消息 ID 在暂停集合中
- 且消息中还有其他 Pending 状态的文件
- 则从暂停集合中移除消息 ID，让下一个文件开始处理

**修改位置**：`ProcessingController.cpp` 的 `finalizeTask` 函数

```cpp
void ProcessingController::finalizeTask(const QString& taskId, const QString& sessionId, const QString& messageId)
{
    // ... 现有代码 ...

    if (!messageId.isEmpty() && !sessionId.isEmpty()) {
        syncMessageProgress(messageId, sessionId);
        syncMessageStatus(messageId, sessionId);
        
        // 检查消息的所有文件是否都已处理完成
        bool allFilesSettled = true;
        bool hasPendingFiles = false;
        const Message msg = messageForSession(sessionId, messageId);
        for (const MediaFile& mf : msg.mediaFiles) {
            if (mf.status == ProcessingStatus::Pending) {
                allFilesSettled = false;
                hasPendingFiles = true;
                break;
            }
            if (mf.status == ProcessingStatus::Processing) {
                allFilesSettled = false;
            }
        }
        
        // 如果所有文件都已处理完成，注销消息的时间管理
        if (allFilesSettled && m_processingTimeManager) {
            m_processingTimeManager->unregisterTask(messageId);
        }
        
        // 【修复】如果消息 ID 在暂停集合中，且还有待处理的文件，移除暂停状态
        // 这样下一个文件可以开始处理
        if (m_pausedMessageIds.contains(messageId)) {
            if (allFilesSettled) {
                // 所有文件都已处理完成，移除暂停状态
                m_pausedMessageIds.remove(messageId);
                qInfo() << "[ProcessingController] Removed completed message from paused set:" << messageId;
            } else if (hasPendingFiles) {
                // 还有待处理的文件，移除暂停状态让下一个文件开始处理
                m_pausedMessageIds.remove(messageId);
                qInfo() << "[ProcessingController] Removed message from paused set to allow next file processing:" << messageId;
            }
        }
    }

    // ... 现有代码 ...
}
```

### 方案二：修改 `processNextTask` 逻辑

在 `processNextTask` 中，对于模式二，检查当前是否有正在处理的任务：
- 如果有正在处理的任务，跳过暂停检查
- 如果没有正在处理的任务，才检查暂停状态

**缺点**：可能破坏模式二的语义（顺序暂停）

## 实施步骤

1. 修改 `ProcessingController.cpp` 的 `finalizeTask` 函数
2. 添加日志输出，便于调试
3. 构建并测试

## 测试用例

1. 添加两个视频文件，选择 Shader 模式
2. 选择模式二（顺序暂停）
3. 不点击任何按钮
4. 验证两个视频都能正常处理完成

## 风险评估

- **低风险**：修改只影响消息 ID 从暂停集合移除的时机
- **不影响现有功能**：模式二的语义保持不变（顺序处理）
- **向后兼容**：不影响模式一和模式三的行为
