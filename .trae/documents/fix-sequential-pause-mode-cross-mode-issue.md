# 顺序暂停模式下不同处理模式切换问题修复计划

## 问题描述

用户报告在顺序暂停模式（pauseMode = 1）下，当 Shader 模式的 A 消息卡片任务处理完成后，处理 AI 推理模式的 B 消息卡片任务时，B 消息卡片任务没有进行处理，而是一直显示等待处理状态。

## 问题分析

### 代码流程分析

1. **`processNextTask` 函数**（第 406-524 行）：
   - 检查 `m_pausedMessageIds` 是否为空
   - 如果 `pauseMode == 1` 且 `m_pausedMessageIds` 不为空，会检查第一个 Pending 任务是否属于暂停的消息
   - 如果是，则直接返回，不处理下一个任务

2. **`finalizeTask` 函数**（第 2557-2618 行）：
   - 在任务完成时被调用
   - 检查消息的所有文件是否都已处理完成（`allFilesSettled`）
   - 如果消息 ID 在暂停集合中，且所有文件都已处理完成，会移除暂停状态
   - **但没有将暂停状态传递给下一个消息**

3. **`cancelMessageTasks` 函数**（第 1774-1865 行）：
   - 当取消消息时，会根据暂停模式将暂停状态传递给下一个消息
   - 这个逻辑只在消息被取消时触发

### 根本原因

在顺序暂停模式（pauseMode = 1）下：
1. 当 A 消息卡片被暂停时，A 的 ID 被添加到 `m_pausedMessageIds`
2. 当 A 消息卡片的所有文件都处理完成后，`finalizeTask` 会移除 A 的 ID
3. 但是，`finalizeTask` 没有将暂停状态传递给下一个消息（B 消息卡片）
4. 这导致 B 消息卡片的任务无法开始处理

### 对比分析

| 场景 | 暂停状态传递 | 结果 |
|------|-------------|------|
| 取消消息 | 有传递逻辑 | 下一个消息被暂停 |
| 消息正常完成 | 无传递逻辑 | 下一个消息无法开始处理 |

## 解决方案

### 方案一：在 `finalizeTask` 中添加暂停状态传递逻辑

在 `finalizeTask` 函数中，当消息的所有文件都处理完成后，如果是顺序暂停模式（pauseMode = 1），将暂停状态传递给下一个消息。

**优点**：
- 保持与 `cancelMessageTasks` 一致的逻辑
- 确保顺序暂停模式的正确行为

**缺点**：
- 需要修改 `finalizeTask` 函数

### 方案二：修改 `processNextTask` 的逻辑

修改 `processNextTask` 函数，当 `m_pausedMessageIds` 为空时，不检查暂停状态，直接处理下一个任务。

**优点**：
- 修改范围较小

**缺点**：
- 可能影响其他暂停模式的逻辑

### 推荐方案

采用方案一，在 `finalizeTask` 中添加暂停状态传递逻辑，与 `cancelMessageTasks` 保持一致。

## 实现步骤

### 步骤 1：修改 `finalizeTask` 函数

在 `finalizeTask` 函数中，当消息的所有文件都处理完成后，添加暂停状态传递逻辑：

```cpp
// 在移除暂停状态后，检查是否需要传递暂停状态
if (wasPaused && allFilesSettled) {
    int pauseMode = SettingsController::instance()->pauseMode();
    if (pauseMode == 1) {  // 顺序暂停模式
        QString nextMessageId = findNextPendingMessageId();
        if (!nextMessageId.isEmpty()) {
            m_pausedMessageIds.insert(nextMessageId);
            qInfo() << "[ProcessingController] Mode 1: Pause state transferred to next message:" << nextMessageId;
            
            // 将下一个消息的首个 Pending 文件标记为暂停
            QString nextSessionId = resolveSessionIdForMessage(nextMessageId);
            for (auto& task : m_tasks) {
                if (task.messageId == nextMessageId && task.status == ProcessingStatus::Pending) {
                    task.status = ProcessingStatus::Paused;
                    updateFileStatusForSessionMessage(nextSessionId, nextMessageId, task.fileId,
                        ProcessingStatus::Paused, QString());
                    qInfo() << "[ProcessingController] Mode 1: First pending file of next message marked as paused:" << task.fileId;
                    break;
                }
            }
            
            // 更新消息状态
            if (m_messageModel) {
                m_messageModel->updateStatus(nextMessageId, static_cast<int>(ProcessingStatus::Paused));
            }
        }
    }
}
```

### 步骤 2：添加日志记录

添加详细的日志记录，方便调试和验证：

```cpp
qInfo() << "[ProcessingController] finalizeTask: messageId:" << messageId
        << "allFilesSettled:" << allFilesSettled
        << "wasPaused:" << wasPaused
        << "pauseMode:" << pauseMode;
```

### 步骤 3：测试验证

测试以下场景：
1. Shader 模式 → AI 推理模式（CPU 子模式）
2. Shader 模式 → AI 推理模式（GPU 子模式）
3. AI 推理模式 → Shader 模式
4. 图片类型 → 视频类型
5. 视频类型 → 图片类型
6. 暂停后恢复处理

## 注意事项

1. **保持一致性**：确保与 `cancelMessageTasks` 中的暂停状态传递逻辑保持一致
2. **边界条件**：处理下一个消息不存在的情况
3. **状态同步**：确保消息状态和文件状态同步更新
4. **日志记录**：添加详细的日志记录，方便调试

## 风险评估

- **低风险**：修改范围较小，逻辑清晰
- **测试覆盖**：需要覆盖各种处理模式切换场景
