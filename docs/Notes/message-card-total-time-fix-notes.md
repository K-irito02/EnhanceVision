# 消息卡片总耗时修复

## 概述

修复消息卡片在会话切换后"总耗时"显示不一致的问题。

**创建日期**: 2026-04-12

---

## 问题描述

当消息卡片任务正在处理时，用户切换到其他会话标签，等处理完成后切换回来，发现：
1. "已耗时"能正常显示
2. "总耗时"未显示或与"已耗时"不一致

**根本原因**：
- 时间计算存在多个独立时间源（`TaskTimeEstimator`、`ProcessingTimeManager`、QML）
- 会话切换时 QML delegate 被销毁/重建，`_actualTotalSec` 重置
- `MessageModel` 在会话切换后只包含当前会话的消息，导致 `updateActualTotalSec` 找不到目标消息

---

## 变更概述

### 修改文件

| 文件路径 | 修改内容 |
|----------|----------|
| `include/EnhanceVision/models/DataTypes.h` | 添加 `totalPausedMs` 字段 |
| `include/EnhanceVision/core/ProcessingTimeManager.h` | 添加 `totalPausedMs`、`lastPauseStartMs` 字段和 `setSessionController` 方法 |
| `src/core/ProcessingTimeManager.cpp` | 统一使用 `processingStartTime` 计算时间，支持跨会话更新 |
| `include/EnhanceVision/core/TaskTimeEstimator.h` | `startTracking` 添加 `startTimeMs` 参数 |
| `src/core/TaskTimeEstimator.cpp` | 接收外部开始时间参数 |
| `include/EnhanceVision/models/MessageModel.h` | 添加 `TotalPausedMsRole` 和 `updateTotalPausedMs` 方法 |
| `src/models/MessageModel.cpp` | 实现 `updateTotalPausedMs` |
| `include/EnhanceVision/controllers/SessionController.h` | 添加 `updateMessageActualTotalSec` 方法 |
| `src/controllers/SessionController.cpp` | 实现跨会话更新 `actualTotalSec` |
| `qml/components/MessageItem.qml` | 简化时间计算逻辑，完全依赖 C++ 持久化值 |

---

## 实现的功能特性

- ✅ **统一时间源**：使用 `processingStartTime` 作为唯一时间源
- ✅ **累积暂停时间**：新增 `totalPausedMs` 字段准确追踪暂停时间
- ✅ **跨会话更新**：`SessionController::updateMessageActualTotalSec` 支持更新非当前会话的消息
- ✅ **持久化支持**：`totalPausedMs` 保存到会话 JSON，支持应用重启恢复
- ✅ **QML 简化**：移除 QML 层的临时计算逻辑，完全依赖 C++ 后端

---

## 技术实现细节

### 核心设计

```
时间计算流程：
processingStartTime (唯一真源)
    ↓
ProcessingTimeManager.unregisterTask()
    ↓
计算: actualTotalSec = (now - processingStartTime - totalPausedMs) / 1000
    ↓
SessionController.updateMessageActualTotalSec() (跨会话更新)
    ↓
Session.messages[i].actualTotalSec (持久化)
    ↓
saveSessions() (保存到 JSON)
```

### 关键代码

```cpp
// ProcessingTimeManager::unregisterTask
qint64 actualTotalSec = 0;
if (info.processingStartTime > 0) {
    const qint64 totalElapsedMs = nowMs - info.processingStartTime;
    const qint64 effectiveElapsedMs = qMax<qint64>(0, totalElapsedMs - totalPaused);
    actualTotalSec = qMax<qint64>(1, qRound64(effectiveElapsedMs / 1000.0));
}

// 优先使用 SessionController 跨会话更新
if (m_sessionController && actualTotalSec > 0) {
    m_sessionController->updateMessageActualTotalSec(messageId, actualTotalSec);
}
```

---

## 测试验证

| 场景 | 预期结果 | 实际结果 |
|------|----------|----------|
| 正常处理（不切换会话） | 总耗时正确显示 | ✅ 通过 |
| 处理中切换会话后返回 | 总耗时正确显示 | ✅ 通过 |
| 应用重启后恢复 | 总耗时正确显示 | ✅ 通过 |
| 暂停/继续后完成 | 总耗时排除暂停时间 | ✅ 通过 |

---

## 后续工作

- [ ] 监控长期稳定性
- [ ] 考虑添加更多时间相关的单元测试
