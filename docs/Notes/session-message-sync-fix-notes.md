# 会话消息同步与任务队列修复

## 概述

**创建日期**: 2026-04-05  
**问题描述**: 修复三个核心问题：会话标签拖拽排序不持久化、多会话间消息混乱、CPU推理任务完成后等待任务不启动

---

## 一、变更概述

### 修改文件
| 文件路径 | 修改内容 |
|----------|----------|
| `src/controllers/SessionController.cpp` | 重构 loadSessions() 批量加载、修复 switchSession() 双重加载、增强 onMessageCountChanged() 一致性校验 |
| `src/controllers/ProcessingController.cpp` | 修复 AI 引擎释放时序、增加任务队列重试机制、failTask() 中同步释放引擎 |

---

## 二、问题分析与解决方案

### 问题1：会话标签拖拽排序不持久化

**根因分析：**
- `loadSessions()` 逐个调用 `addSession()`
- `addSession()` 每次都将会话插入到"置顶会话之后"的固定位置
- 然后调用 `updateSortIndices()` 覆盖掉已保存的 `sortIndex`
- 导致无论保存了什么排序，加载后总会被重置

**解决方案：**
```cpp
// 批量加载会话并按 sortIndex 排序，保留用户拖拽排序结果
QList<Session> allSessions;
for (const QJsonValue& value : sessionsArray) {
    Session session = jsonToSession(value.toObject());
    allSessions.append(session);
}

// 按 sortIndex 排序（置顶会话在前，普通会话在后，同组内按 sortIndex 升序）
std::sort(allSessions.begin(), allSessions.end(),
          [](const Session& a, const Session& b) {
              if (a.isPinned != b.isPinned) {
                  return a.isPinned > b.isPinned;
              }
              return a.sortIndex < b.sortIndex;
          });

// 批量加载到 SessionModel
m_sessionModel->updateSessions(allSessions);
```

### 问题2：多会话间消息混乱

**根因分析（多点）：**
1. `switchSession()` 中 `m_sessionModel->switchSession()` 会触发 `activeSessionChanged` 信号，之后又手动调用 `loadSessionMessages()`，导致双重加载/同步冲突
2. `onMessageCountChanged()` 在删除消息后无条件将 MessageModel 同步回 Session，没有校验 sessionId 一致性
3. `deleteSession()` 和 `deleteSelectedSessions()` 也存在类似的双重加载问题

**解决方案：**

1. **重构 switchSession()**：移除冗余的手动加载，统一由 `activeSessionChanged` 信号处理器管理
```cpp
void SessionController::switchSession(const QString& sessionId)
{
    // ...
    saveCurrentSessionMessages();
    
    // 切换会话（会触发 activeSessionChanged 信号）
    // 信号处理器会自动执行：保存旧会话消息 + 加载新会话消息
    // 【修复】不再在这里手动调用 loadSessionMessages()
    m_sessionModel->switchSession(sessionId);
    
    emit sessionSwitched(sessionId);
}
```

2. **增强 onMessageCountChanged()**：增加 sessionId 一致性校验
```cpp
void SessionController::onMessageCountChanged()
{
    // ...
    // 校验 sessionId 一致性：只有当 MessageModel 的 sessionId 
    // 与当前活动会话一致时才同步，防止跨会话数据污染
    const QString activeId = m_sessionModel->activeSessionId();
    if (currentId != activeId) {
        qWarning() << "sessionId mismatch - skipping sync";
        return;
    }
    // ...
}
```

3. **修复 deleteSession() 和 deleteSelectedSessions()**：移除手动 loadSessionMessages 调用

### 问题3：CPU推理完成后等待任务不启动

**根因分析：**
- AI 引擎释放在 `terminateTask()` 中通过 `QTimer::singleShot(0, ...)` 延迟执行
- 当 `finalizeTask()` → `processNextTask()` 时，引擎还未释放
- `availableCount() <= 0` 导致任务被跳过，且没有后续重试机制

**解决方案：**

1. **同步释放引擎**：在 `completeTask()` 和 `failTask()` 中，AI 推理任务完成/失败后立即同步释放引擎
```cpp
// AI 图像推理完成后，同步释放引擎
// 确保 finalizeTask → processNextTask 时引擎已可用
disconnectAiEngineForTask(taskId);
m_aiEnginePool->release(taskId);
```

2. **增加延迟重试机制**：当 AI 引擎暂时不可用时，安排延迟重试
```cpp
if (!nextTask) {
    // 如果有等待 AI 引擎的任务被跳过，安排延迟重试
    if (hasSkippedAITask) {
        QTimer::singleShot(200, this, &ProcessingController::processNextTask);
    }
    return;
}
```

---

## 三、实现的功能特性

- ✅ **会话排序持久化**：拖拽排序后关闭重开保持顺序
- ✅ **消息隔离**：多会话间切换不再混乱
- ✅ **任务连续处理**：CPU 推理任务完成后自动处理下一个
- ✅ **引擎同步释放**：确保任务完成/失败后引擎立即可用
- ✅ **延迟重试机制**：引擎暂时不可用时自动重试
- ✅ **索引清理**：删除消息后清理残留的 messageToSessionId 索引

---

## 四、测试验证

| 场景 | 预期结果 | 实际结果 |
|------|----------|----------|
| 拖拽排序后重启 | 保持排序 | ✅ 通过 |
| 多会话切换 | 消息不混乱 | ✅ 通过 |
| 删除消息后切换 | 消息不混乱 | ✅ 通过 |
| CPU 连续推理 | 自动处理下一个 | ✅ 通过 |
| 任务失败后继续 | 自动处理下一个 | ✅ 通过 |

---

## 五、关键设计原则

1. **单一信号源**：会话切换统一由 `activeSessionChanged` 信号处理器管理，避免多处手动调用导致的不一致
2. **一致性校验**：所有跨会话操作前校验 sessionId，防止数据污染
3. **同步释放资源**：关键资源（如 AI 引擎）在任务结束时同步释放，而非延迟释放
4. **防御性重试**：资源暂时不可用时安排延迟重试，增强健壮性

---

## 六、后续工作

- [ ] 监控长期稳定性
- [ ] 考虑添加更多的会话操作日志
- [ ] 优化大量会话时的加载性能
