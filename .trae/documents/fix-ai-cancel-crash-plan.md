# AI 推理模式取消任务导致程序闪退 - 修复计划

## 问题现象

在 AI 推理模式下处理视频时，用户取消/删除正在运行的任务后，程序界面闪退（崩溃）。

---

## 已完成的修复（✅ 代码已确认）

### 修复 1：崩溃问题（Use-After-Free）
- **文件**: [ProcessingController.cpp](src/controllers/ProcessingController.cpp#L1665-L1704)
- **状态**: ✅ 已完成
- **内容**:
  - `cleanupTask()` 不再 `take()` AIVideoProcessor，保持引用在 map 中
  - 移除 `waitForFinished()` 阻塞调用
  - 所有重量级操作通过 `QTimer::singleShot(0)` 延迟到下一事件循环
  - 新增 `handleOrphanedVideoTask()` 兜底清理（15秒超时）

### 修复 2：级联误删问题（多任务互相影响）
- **文件**: [ProcessingController.cpp](src/controllers/ProcessingController.cpp) 的 `cancelTask()`
- **状态**: ✅ 已完成
- **内容**: 移除 messageId 自动检测逻辑，仅按 taskId 精确匹配取消单个任务

### 修复 3：后续任务失败（奇偶规律）
- **文件**: [AIVideoProcessor.cpp](src/core/video/AIVideoProcessor.cpp)、[AIEnginePool.cpp](src/core/AIEnginePool.cpp)、[AIInferenceWorker.cpp](src/core/ai/AIInferenceWorker.cpp)、[ProcessingEngine.cpp](src/core/ProcessingEngine.cpp)
- **状态**: ✅ 已完成
- **内容**:
  - 移除所有位置的 `engine->cancelProcess()` 调用（共5处）
  - `AIEnginePool::acquire()` 改用 `resetState()` 重置引擎状态
  - 避免引擎内部 NCNN 状态被污染

### 修复 4：状态跳转问题
- **文件**: [ProcessingController.cpp](src/controllers/ProcessingController.cpp) 的 completed 信号槽
- **状态**: ✅ 已完成
- **内容**: 增加 `taskAlreadyRemoved` 检测，防止已删除任务的延迟信号触发重复清理

### 修复 5：卡顿优化（C++ 层异步化）
- **文件**: [ProcessingController.cpp](src/controllers/ProcessingController.cpp#L1686-L1703)、[MessageModel.cpp](src/models/MessageModel.cpp#L832-L859)
- **状态**: ✅ 已完成
- **内容**:
  - `cleanupTask()` 所有重量级操作包装在 `QTimer::singleShot(0)` 中
  - `removeMediaFile()` 全部操作包装在 `QTimer::singleShot(0)` 中

### 修复 6：卡顿优化（QML 层异步化）
- **文件**: [MessageItem.qml](qml/components/MessageItem.qml#L368)
- **状态**: ✅ 已完成
- **内容**: `onDeleteFile` 改用 `Qt.callLater()` 延迟执行

---

## 🚨 发现的遗留问题（需立即修复）

### 问题 A：`MediaThumbnailStrip.qml` 中 `fileRemoved` 信号仍被发射（关键！）

**严重程度**: 🔴 高 — 可能重新引入级联误删问题

**位置**: [MediaThumbnailStrip.qml](qml/components/MediaThumbnailStrip.qml)

| 行号 | 触发方式 |
|------|----------|
| L656 | 失败状态的缩略图删除按钮 |
| L999 | 正常状态的缩略图删除按钮 |
| L1036 | 失败状态的缩略图删除按钮（展开模式） |
| L1361 | 右键菜单删除选项 |

**问题代码示例** (L995-L1001):
```qml
onClicked: {
    var origIndex = thumbDelegate.itemData ? thumbDelegate.itemData.origIndex : thumbDelegate.index
    root.deleteFile(origIndex)          // ← 第一次删除
    if (root.messageId !== "") {
        root.fileRemoved(root.messageId, origIndex)  // ← 发射信号（应移除！）
    }
}
```

**风险分析**:
- 虽然 `MessageItem.qml` 已移除 `onFileRemoved` 处理器，但信号仍在发射
- 如果有其他组件连接了此信号，可能导致意外的重复删除
- 信号发射本身虽不阻塞，但属于冗余逻辑，违反"最小变更原则"

**修复方案**: 移除所有 4 处 `root.fileRemoved(...)` 调用，仅保留 `root.deleteFile(origIndex)`

---

### 问题 B：`handleOrphanedVideoTask()` 中重复调用 `release()`

**严重程度**: 🟡 中 — 可能导致引擎池状态异常

**位置**: [ProcessingController.cpp](src/controllers/ProcessingController.cpp#L1178-L1179)

```cpp
m_aiEnginePool->release(taskId);   // L1178: 第一次 release
m_aiEnginePool->release(taskId);   // L1179: 重复 release！（应删除）
```

**修复方案**: 删除第 1179 行的重复调用

---

## 待执行修复清单

| # | 问题 | 文件 | 操作 |
|---|------|------|------|
| 1 | 移除 `fileRemoved` 信号发射（4处） | MediaThumbnailStrip.qml L656, L999, L1036, L1361 | 删除 `root.fileRemoved(...)` 行 |
| 2 | 删除重复 `release()` 调用 | ProcessingController.cpp L1179 | 删除重复行 |

---

## 验证步骤

1. 启动程序，开始一个 AI 视频处理任务（使用较长的视频）
2. 在处理过程中（至少等第一帧推理开始后）点击取消/删除任务
3. **预期结果**：
   - ✅ 任务正常取消，程序不崩溃
   - ✅ UI 无卡顿（点击瞬间响应流畅）
   - ✅ 多任务场景下删除一个不影响其他任务
   - ✅ 后续任务能正常处理完成
4. 检查 `runtime_output.log`：无崩溃堆栈、无警告
5. 连续多次取消→重新处理，验证无资源泄漏
