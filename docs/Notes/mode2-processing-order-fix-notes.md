# 模式2处理顺序修复

## 概述

**创建日期**: 2026-04-07  
**相关功能**: 任务控制模式2（自由选择模式）

---

## 一、变更概述

### 修改文件
| 文件路径 | 修改内容 |
|----------|----------|
| `src/controllers/ProcessingController.cpp` | 修复模式2的处理顺序和后续消息处理逻辑 |
| `docs/Plan/任务控制模式详解.md` | 更新文档，添加处理顺序保证说明 |

---

## 二、修复的问题

### 问题1：激活顺序不正确
- **现象**：激活A、B、C后，处理顺序是A->C->B，而不是A->B->C
- **原因**：在 `resumeMessageTasks()` 中，当有消息正在处理时，新激活的消息被插入到当前处理消息之后，而不是队列末尾
- **修复**：简化逻辑，所有激活的消息都添加到队列末尾，保持激活顺序

### 问题2：A恢复处理完后，后续消息不继续处理
- **现象**：激活A、B、C、D、E后，暂停A再恢复，A处理完后后续消息不处理
- **原因**：优先队列为空时，已激活消息的任务仍然是 Paused 状态，无法被 `processNextTask()` 处理
- **修复**：在 `processNextTask()` 中，当优先队列为空但有已激活消息时，将它们的 Paused 任务恢复为 Pending

---

## 三、技术实现细节

### 关键修改点

#### 1. 简化队列添加逻辑
```cpp
// 添加到优先恢复队列（如果不存在）
// 始终添加到队列末尾，保持激活/恢复顺序
if (!m_priorityResumeMessageIds.contains(messageId)) {
    m_priorityResumeMessageIds.append(messageId);
}
```

#### 2. 查找优先消息时同时检查 Pending 和 Paused 状态
```cpp
// 检查该消息是否还有待处理的任务（Pending 或 Paused 状态）
for (auto& task : m_tasks) {
    if (task.messageId == msgId && 
        (task.status == ProcessingStatus::Pending || task.status == ProcessingStatus::Paused)) {
        priorityMessageId = msgId;
        break;
    }
}
```

#### 3. 优先队列为空时恢复已激活消息的任务
```cpp
// 如果优先队列为空，但有已激活的消息，确保它们的任务是 Pending 状态
if (m_priorityResumeMessageIds.isEmpty() && !m_activatedMessageIds.isEmpty()) {
    for (const QString& activatedMsgId : m_activatedMessageIds) {
        if (m_pausedMessageIds.contains(activatedMsgId)) continue;
        for (auto& task : m_tasks) {
            if (task.messageId == activatedMsgId && task.status == ProcessingStatus::Paused) {
                task.status = ProcessingStatus::Pending;
                // 更新UI状态
            }
        }
    }
}
```

---

## 四、处理顺序保证

### 首次激活场景
```
用户依次激活A、B、C
    ↓
优先恢复队列：[A, B, C]（按激活顺序）
    ↓
处理顺序：A → B → C（严格按激活顺序）
```

### 暂停后恢复场景
```
激活A、B、C、D、E → 暂停A → B开始处理 → 恢复A
    ↓
优先恢复队列：[A, B, C, D, E, A]（A添加到末尾）
    ↓
处理顺序：B → C → D → E → A
```

---

## 五、测试验证

| 场景 | 预期结果 | 实际结果 |
|------|----------|----------|
| 激活A、B、C | 处理顺序A→B→C | ✅ 通过 |
| 激活A、B、C，暂停A后恢复 | A处理完后继续处理后续消息 | ✅ 通过 |
| 暂停后恢复的消息添加到队列末尾 | 不会插队 | ✅ 通过 |
| A暂停→B处理→恢复A | B的所有文件完成后才处理A | ✅ 通过 |

---

## 六、补充修复（2026-04-07）

### 问题3：恢复A后，B处理一个文件就切换到A
- **现象**：A暂停后恢复，B处理完一个文件后就开始处理A，而不是等B全部完成
- **原因**：`processNextTask()` 中的等待逻辑检查条件不正确，没有正确判断当前消息是否还有待处理任务
- **修复**：修改条件为 `!m_currentProcessingMessageId.isEmpty() && currentMessageHasPendingTasks`

```cpp
// 修复前：检查当前消息是否在优先队列中
if (!m_currentProcessingMessageId.isEmpty() && 
    !m_priorityResumeMessageIds.contains(m_currentProcessingMessageId))

// 修复后：检查当前消息是否还有待处理任务
if (!m_currentProcessingMessageId.isEmpty() && currentMessageHasPendingTasks)
```

---

## 七、清理工作

- 删除了任务级别的冗余调试日志
- 保留了消息级别的关键日志
- 简化了代码注释
