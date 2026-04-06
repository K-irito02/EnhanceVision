# 顺序暂停模式优化计划

## 问题描述

用户报告了两个问题：

### 问题1：暂停图标显示问题
1. 当A消息卡片是暂停状态时，逐步删除完A中的所有文件后，B消息卡片的第一个文件没有显示暂停图标
2. 某些情况下会有多个文件显示暂停图标（应该只有一个）

### 问题2：新消息卡片的等待状态问题
当一个会话标签中存在被暂停的消息卡片时，新上传的消息卡片需要变成对应的等待处理状态，处理过程需要和第二种模式（顺序暂停）一致

---

## 问题分析

### 问题1分析

**根本原因1**：`processDeletionBatch` 函数（第2021-2036行）在删除最后一个文件时，只更新了消息状态为暂停，但没有将下一个消息的第一个文件标记为暂停状态。

对比 `finalizeTask` 函数（第2630-2640行）：
```cpp
// 将下一个消息的首个 Pending 文件标记为暂停
QString nextSessionId = resolveSessionIdForMessage(nextMessageId);
for (auto& task : m_tasks) {
    if (task.messageId == nextMessageId && task.status == ProcessingStatus::Pending) {
        task.status = ProcessingStatus::Paused;
        updateFileStatusForSessionMessage(nextSessionId, nextMessageId, task.fileId,
            ProcessingStatus::Paused, QString());
        break;
    }
}
```

`finalizeTask` 正确地将下一个消息的第一个待处理文件标记为暂停状态，但 `processDeletionBatch` 缺少这个逻辑。

**根本原因2**：`processDeletionBatch` 函数（第2006-2016行）在删除暂停状态的文件时，会将暂停状态传递给下一个待处理文件，但没有检查当前消息中是否已经有文件是暂停状态。

### 问题2分析

**根本原因**：`addTask` 和 `createAndRegisterTask` 函数没有检查是否存在暂停的消息卡片。在顺序暂停模式（pauseMode == 1）下，如果存在暂停的消息卡片，新消息卡片应该等待而不是立即开始处理。

---

## 修复方案

### 修复问题1：暂停图标显示问题

#### 修复点1：`processDeletionBatch` 函数 - 传递暂停状态时标记文件

在 `processDeletionBatch` 函数的第2021-2036行，当传递暂停状态给下一个消息时，需要将下一个消息的第一个 Pending 文件标记为暂停状态。

**修改位置**：`ProcessingController.cpp` 第2021-2036行

**修改内容**：
```cpp
} else if (!hasRemainingPendingTasks) {
    // 消息没有待处理任务了，移除暂停状态并传递给下一个消息
    m_pausedMessageIds.remove(messageId);
    qInfo() << "[ProcessingController] Removed message with no remaining tasks from paused set:" << messageId;
    
    // 【修复】暂停状态传递：查找下一个待处理的消息并将其加入暂停集合
    QString nextMessageId = findNextPendingMessageId();
    if (!nextMessageId.isEmpty()) {
        m_pausedMessageIds.insert(nextMessageId);
        qInfo() << "[ProcessingController] Pause state transferred to next message:" << nextMessageId;
        if (m_messageModel) {
            m_messageModel->updateStatus(nextMessageId, static_cast<int>(ProcessingStatus::Paused));
        }
        
        // 【新增】将下一个消息的首个 Pending 文件标记为暂停
        QString nextSessionId = resolveSessionIdForMessage(nextMessageId);
        for (auto& task : m_tasks) {
            if (task.messageId == nextMessageId && task.status == ProcessingStatus::Pending) {
                task.status = ProcessingStatus::Paused;
                updateFileStatusForSessionMessage(nextSessionId, nextMessageId, task.fileId,
                    ProcessingStatus::Paused, QString());
                qInfo() << "[ProcessingController] First pending file of next message marked as paused:" << task.fileId;
                break;
            }
        }
        
        // 暂停状态已传递，不启动新任务
        return;
    }
}
```

#### 修复点2：`processDeletionBatch` 函数 - 确保只有一个文件显示暂停图标

在 `processDeletionBatch` 函数的第2006-2016行，传递暂停状态之前，需要检查当前消息中是否已经有文件是暂停状态。

**修改位置**：`ProcessingController.cpp` 第1990-2038行

**修改内容**：
```cpp
// 【修复】检查消息是否还有剩余任务
if (m_pausedMessageIds.contains(messageId)) {
    // 【新增】先检查当前消息中是否已经有文件是暂停状态
    bool hasPausedFile = false;
    for (const auto& task : m_tasks) {
        if (task.messageId == messageId && task.status == ProcessingStatus::Paused) {
            hasPausedFile = true;
            break;
        }
    }
    
    bool hasRemainingPendingTasks = false;
    QString nextPendingFileId;
    
    for (const auto& task : m_tasks) {
        if (task.messageId == messageId && task.status == ProcessingStatus::Pending) {
            hasRemainingPendingTasks = true;
            if (nextPendingFileId.isEmpty()) {
                nextPendingFileId = task.fileId;
            }
            break;
        }
    }
    
    if (hasRemainingPendingTasks && !nextPendingFileId.isEmpty() && !hasPausedFile) {
        // 【修改】只有在没有暂停文件时才传递暂停状态
        // 将暂停状态传递给下一个待处理文件
        QString sessionId = resolveSessionIdForMessage(messageId);
        for (auto& task : m_tasks) {
            if (task.messageId == messageId && task.fileId == nextPendingFileId) {
                task.status = ProcessingStatus::Paused;
                updateFileStatusForSessionMessage(sessionId, messageId, nextPendingFileId,
                    ProcessingStatus::Paused, QString());
                qInfo() << "[ProcessingController] Pause status transferred to next file:" << nextPendingFileId;
                break;
            }
        }
        
        // 消息仍有待处理任务，保持暂停状态，不启动新任务
        qInfo() << "[ProcessingController] Message still has pending tasks, keeping paused:" << messageId;
        return;
    } else if (!hasRemainingPendingTasks) {
        // ... 后续代码不变
    }
}
```

### 修复问题2：新消息卡片的等待状态问题

#### 修复点3：`addTask` 函数 - 检查暂停状态

在 `addTask` 函数中，需要检查是否存在暂停的消息卡片。在顺序暂停模式（pauseMode == 1）下，如果存在暂停的消息卡片，新消息卡片应该等待。

**修改位置**：`ProcessingController.cpp` 第270-290行

**修改内容**：
```cpp
QString ProcessingController::addTask(const Message& message)
{
    QString sessionId;
    if (m_sessionController) {
        sessionId = m_sessionController->sessionIdForMessage(message.id);
        if (sessionId.isEmpty()) {
            sessionId = m_sessionController->activeSessionId();
        }
    }
    
    // 【新增】检查是否需要将新消息设置为等待状态
    int pauseMode = SettingsController::instance()->pauseMode();
    bool shouldWait = false;
    
    if (pauseMode == 1) {  // 顺序暂停模式
        // 检查是否有正在处理的任务
        if (m_currentProcessingCount > 0) {
            shouldWait = true;
        }
        // 检查是否有暂停的消息卡片
        if (!m_pausedMessageIds.isEmpty()) {
            shouldWait = true;
        }
        // 检查是否有其他待处理的消息（FIFO顺序）
        for (const auto& task : m_tasks) {
            if (task.status == ProcessingStatus::Pending || task.status == ProcessingStatus::Paused) {
                if (task.messageId != message.id) {
                    shouldWait = true;
                    break;
                }
            }
        }
    }
    
    for (const auto& mediaFile : message.mediaFiles) {
        createAndRegisterTask(message, mediaFile, sessionId);
    }

    // 【新增】如果需要等待，更新消息状态为 Pending（等待处理）
    if (shouldWait && m_messageModel) {
        m_messageModel->updateStatus(message.id, static_cast<int>(ProcessingStatus::Pending));
        qInfo() << "[ProcessingController] New message set to waiting state:" << message.id;
    }

    updateQueuePositions();
    emit queueSizeChanged();

    processNextTask();

    return message.id;
}
```

---

## 实施步骤

1. **修复问题1.1**：在 `processDeletionBatch` 函数中，当传递暂停状态给下一个消息时，将下一个消息的第一个 Pending 文件标记为暂停状态
2. **修复问题1.2**：在 `processDeletionBatch` 函数中，传递暂停状态之前，检查当前消息中是否已经有文件是暂停状态
3. **修复问题2**：在 `addTask` 函数中，检查是否存在暂停的消息卡片，如果存在，将新消息设置为等待状态
4. **构建验证**：使用 `qt-build-and-fix` 技能构建项目
5. **测试验证**：测试各种场景

---

## 测试场景

### 问题1测试
1. 创建A消息卡片（多个文件），设置为暂停状态
2. 逐步删除A中的所有文件
3. 验证B消息卡片的第一个文件显示暂停图标
4. 验证一个消息卡片中只有一个文件显示暂停图标

### 问题2测试
1. 创建A消息卡片，设置为暂停状态
2. 上传新文件并发送消息（B消息卡片）
3. 验证B消息卡片显示等待处理状态
4. 恢复A消息卡片，验证B消息卡片开始处理
