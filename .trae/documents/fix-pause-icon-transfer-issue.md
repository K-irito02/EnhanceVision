# 修复模式二（顺序暂停）删除文件后暂停图标未传递问题

## 问题描述

在模式二（顺序暂停）下，当暂停A消息卡片任务后：
- 正在处理的文件a缩略图上显示了暂停图标 ✓
- 删除文件a后，文件b的缩略图上**没有显示**暂停图标 ✗
- 删除文件b后，文件c的缩略图**应该显示**暂停图标

**预期行为：** 当删除暂停状态的文件后，下一个待处理文件应该自动显示暂停图标，因为恢复时第一个处理的文件就是按顺序来的。

## 问题根源分析

### 代码位置

[ProcessingController.cpp](file:///e:/QtAudio-VideoLearning/EnhanceVision/src/controllers/ProcessingController.cpp) 的 `processDeletionBatch` 函数（第1772-1898行）

### 当前逻辑

```cpp
// 第1863-1895行
if (m_pausedMessageIds.contains(messageId)) {
    bool hasRemainingPendingTasks = false;
    for (const auto& task : m_tasks) {
        if (task.messageId == messageId && task.status == ProcessingStatus::Pending) {
            hasRemainingPendingTasks = true;
            break;
        }
    }
    
    if (hasRemainingPendingTasks) {
        // 消息仍有待处理任务，保持暂停状态，不启动新任务
        qInfo() << "[ProcessingController] Message still has pending tasks, keeping paused:" << messageId;
        // 不调用 processNextTask，因为暂停状态应该保持
        return;
    } else {
        // 消息没有待处理任务了，移除暂停状态并传递给下一个消息
        m_pausedMessageIds.remove(messageId);
        // ... 传递给下一个消息
    }
}
```

### 问题所在

当删除暂停的文件a后：
1. 代码检查消息是否还有待处理任务 ✓
2. 如果有，保持暂停状态 ✓
3. **但是没有将暂停状态传递给下一个文件（b文件）** ✗

结果：
- 消息A仍然在 `m_pausedMessageIds` 集合中
- 但是文件b的状态仍然是 `Pending`，而不是 `Paused`
- 所以文件b的缩略图上没有显示暂停图标

### 对比：正确的实现

在 `cancelAndRemoveTask` 函数（第2951-2992行）中，已经实现了正确的逻辑：

```cpp
// 第2973-2985行
if (m_pausedMessageIds.contains(messageId) && !nextPendingFileId.isEmpty()) {
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
}
```

这段代码：
1. 检查消息是否在暂停集合中
2. 查找下一个待处理的文件
3. 将该文件的状态设置为 `Paused`
4. 更新文件状态，让UI显示暂停图标

## 修复方案

### 修改位置

[ProcessingController.cpp](file:///e:/QtAudio-VideoLearning/EnhanceVision/src/controllers/ProcessingController.cpp) 的 `processDeletionBatch` 函数

### 修改内容

在第1863-1895行的逻辑中，添加暂停状态传递给下一个文件的代码：

```cpp
// 【修复】检查消息是否还有剩余任务
if (m_pausedMessageIds.contains(messageId)) {
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
    
    if (hasRemainingPendingTasks && !nextPendingFileId.isEmpty()) {
        // 【新增】将暂停状态传递给下一个待处理文件
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
        // 消息没有待处理任务了，移除暂停状态并传递给下一个消息
        m_pausedMessageIds.remove(messageId);
        // ... 传递给下一个消息
    }
}
```

## 实施步骤

1. **修改 `processDeletionBatch` 函数**
   - 在检查消息是否有剩余任务时，同时记录下一个待处理文件的ID
   - 如果有剩余任务且消息在暂停集合中，将下一个待处理文件的状态设置为 `Paused`
   - 更新文件状态，让UI显示暂停图标

2. **构建验证**
   - 使用 `qt-build-and-fix` 技能构建项目
   - 确保编译无错误

3. **功能测试**
   - 启动应用程序
   - 创建包含多个文件的消息
   - 使用模式二（顺序暂停）暂停消息
   - 删除正在处理的文件，验证下一个文件是否显示暂停图标
   - 重复删除，验证暂停图标是否正确传递

## 影响范围

- **影响文件：** [ProcessingController.cpp](file:///e:/QtAudio-VideoLearning/EnhanceVision/src/controllers/ProcessingController.cpp)
- **影响功能：** 模式二（顺序暂停）的文件删除逻辑
- **影响范围：** 仅影响暂停状态的文件删除，不影响其他功能

## 风险评估

- **风险等级：** 低
- **原因：** 修改仅涉及暂停状态的传递逻辑，不影响核心处理流程
- **回滚方案：** 如果出现问题，可以回滚到原始代码

## 测试要点

1. **基本功能测试**
   - 暂停消息后删除文件，验证暂停图标是否传递
   - 连续删除多个文件，验证暂停图标是否正确传递

2. **边界条件测试**
   - 删除最后一个文件，验证暂停状态是否正确移除
   - 删除所有文件，验证消息状态是否正确更新

3. **模式兼容性测试**
   - 模式一（单任务暂停）：验证不受影响
   - 模式三（自由选择）：验证不受影响

## 预期结果

修复后，在模式二（顺序暂停）下：
- 删除暂停的文件a后，文件b的缩略图上显示暂停图标 ✓
- 删除文件b后，文件c的缩略图上显示暂停图标 ✓
- 恢复时，第一个处理的文件就是按顺序来的 ✓
