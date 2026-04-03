# 修复计划：连续快速点击缩略图删除按钮导致崩溃

## 问题描述
连续快速点击消息卡片中缩略图右上角的 `X` 删除按钮后，应用程序崩溃。
- **影响范围**：Shader 模式和 AI 推理模式均受影响；正在处理、等待、失败状态均会出现

## 根因分析

### 调用链路
```
用户点击 X → MediaThumbnailStrip.deleteBtnMouse.onClicked
  → root.deleteFile(origIndex) [信号]
  → MessageItem.onDeleteFile: Qt.callLater(messageModel.removeMediaFile(msgId, idx))
    → MessageModel::removeMediaFile(): QTimer::singleShot(0, lambda) 异步执行
      → cancelMessageFileTasks(msgId, fileId)
        → 遍历 m_tasks → cancelAndRemoveTask() → cleanupTask(taskId) ← ⚠️ 无去重！
      → mediaFiles.removeAt(fileIndex)
```

### 核心根因：`cleanupTask` 被重复调用（无幂等性保护）

**时序图（快速连击 3 次为例）**：
```
时间线:
[用户连击3次] → Qt.callLater队列: [remove(fIdx1), remove(fIdx2), remove(fIdx3)]
                ↓ 同一事件循环迭代内同步执行
[removeMediaFile #1] → 验证通过 → 排队 singleShot(lambda1)
[removeMediaFile #2] → 验证通过 → 排队 singleShot(lambda2)  ← 索引可能已漂移!
[removeMediaFile #3] → 验证通过 → 排队 singleShot(lambda3)  ← 索引可能已漂移!
                ↓ 下一事件循环
[lambda1 执行]     → cleanupTask(taskId) 第1次 ✅
                     → 创建 15s 兜底定时器 #1
[lambda2 执行]     → cleanupTask(taskId) 第2次 ⚠️ 重复!
                     → 创建 15s 兜底定时器 #2 (泄漏!)
[lambda3 执行]     → cleanupTask(taskId) 第3次 ⚠️ 重复!
                     → 创建 15s 兜底定时器 #3 (泄漏!)
```

**具体危害**：
| 问题 | 后果 |
|------|------|
| `cleanupTask` 重复调用 | N 次调用创建 N 个 15s 兜底定时器，定时器泄漏 |
| `processor->cancel()` 多次 | 可能干扰正在运行的异步处理线程 |
| `m_aiEnginePool->release()` 多次 | 引擎状态被重置多次，可能影响后续任务 |
| 索引漂移 | 删除错误的文件或越界访问 |
| 兜底定时器竞态 | `take()` 最后一个引用导致对象删除，原线程 Use-After-Free |

---

## 修复方案

### 修复 1：`cleanupTask` 添加幂等性保护（核心修复）

**文件**: `src/controllers/ProcessingController.cpp`

使用 `QSet<QString>` 追踪正在清理中的 taskId，防止重复进入：

```cpp
// 在 ProcessingController.h 中新增成员:
QSet<QString> m_cleaningUpTaskIds;

// cleanupTask() 开头添加:
void ProcessingController::cleanupTask(const QString& taskId)
{
    // 幂等性保护：防止快速连续调用导致重复清理
    if (m_cleaningUpTaskIds.contains(taskId)) {
        qWarning() << "[ProcessingController] cleanupTask already in progress:" << taskId;
        return;
    }
    m_cleaningUpTaskIds.insert(taskId);

    // ... 原有逻辑 ...

    QTimer::singleShot(0, this, [this, taskId]() {
        // ... 异步清理操作 ...

        // 清理完成，移除保护标记
        m_cleaningUpTaskIds.remove(taskId);
    });
}
```

### 修复 2：`cancelMessageFileTasks` 添加去重保护

**文件**: `src/controllers/ProcessingController.cpp`

```cpp
void ProcessingController::cancelMessageFileTasks(const QString& messageId, const QString& fileId)
{
    if (messageId.isEmpty() || fileId.isEmpty()) {
        return;
    }

    // 构造去重键：messageId + fileId 组合
    QString dedupeKey = messageId + ":" + fileId;

    // 使用静态/成员 QSet 防止同一文件的重复取消请求
    static QSet<QString> s_pendingCancels;  // 或改为成员变量
    if (s_pendingCancels.contains(dedupeKey)) {
        return;  // 已有相同的取消请求在处理中
    }
    s_pendingCancels.insert(dedupeKey);

    // ... 原 ...

    // 在操作完成后清除标记
    QTimer::singleShot(0, this, [dedupeKey]() {
        s_pendingCancels.remove(dedupeKey);
    });
}
```

### 修复 3：QML 层防抖 - 删除按钮禁用机制

**文件**: `qml/components/MediaThumbnailStrip.qml`

为删除按钮添加点击后立即禁用的防抖机制：

```qml
// 在 thumbDelegate 中添加属性:
property bool _deleteInProgress: false

// deleteBtnMouse / deleteBtnForFailedMouse 的 onClicked 修改为:
MouseArea {
    id: deleteBtnMouse
    anchors.fill: parent
    hoverEnabled: true
    cursorShape: Qt.PointingHandCursor
    preventStealing: true
    enabled: !thumbDelegate._deleteInProgress  // 禁用防抖
    onClicked: {
        thumbDelegate._deleteInProgress = true  // 标记删除进行中
        var origIndex = thumbDelegate.itemData ? thumbDelegate.itemData.origIndex : thumbDelegate.index
        root.deleteFile(origIndex)
        // 500ms 后重新启用（足够让事件循环处理完）
        debounceTimer.start()
    }
}

Timer {
    id: debounceTimer
    interval: 500
    repeat: false
    onTriggered: thumbDelegate._deleteInProgress = false
}
```

注意：horizontalComponent 和 gridComponent 两处 delegate 都需要修改。

### 修复 4：MessageModel 层使用 fileId 替代索引（增强健壮性）

**文件**: `qml/components/MessageItem.qml` + `src/models/MessageModel.cpp`

将 QML 层的 `onDeleteFile` 回调从传递 index 改为传递 fileId：

```qml
// MessageItem.qml 修改 onDeleteFile:
onDeleteFile: function(index) {
    var _msgId = root.taskId
    var _fileId = ""
    if (root.mediaFiles && index >= 0 && index < root.mediaFiles.count) {
        var item = root.mediaFiles.get(index)
        if (item) _fileId = item.id
    }
    var __msgId = _msgId
    var __fileId = _fileId
    Qt.callLater(function() {
        if (__fileId) {
            messageModel.removeMediaFileById(__msgId, __fileId)
        }
    })
}
```

`removeMediaFileById` 使用 fileId 匹配而非索引，天然避免索引漂移问题。

### 修复 5：兜底定时器去重

**文件**: `src/controllers/ProcessingController.cpp`

确保每个 taskId 最多只有一个活跃的兜底定时器：

```cpp
// 成员变量:
QMap<QString, QTimer*> m_orphanTimers;

// cleanupTask 的异步 lambda 中修改:
if (m_activeAIVideoProcessors.contains(taskId)) {
    // 先清除已有的旧定时器
    if (m_orphanTimers.contains(taskId)) {
        QTimer* oldTimer = m_orphanTimers.take(taskId);
        if (oldTimer) {
            oldTimer->stop();
            oldTimer->deleteLater();
        }
    }

    QTimer* orphanTimer = new QTimer(this);
    orphanTimer->setSingleShot(true);
    orphanTimer->setInterval(15000);
    connect(orphanTimer, &QTimer::timeout, this, [this, taskId]() {
        m_orphanTimers.remove(taskId);
        handleOrphanedVideoTask(taskId);
    });
    m_orphanTimers[taskId] = orphanTimer;
    orphanTimer->start();
}
```

---

## 实施步骤

| 步骤 | 内容 | 文件 | 优先级 |
|------|------|------|--------|
| 1 | `cleanupTask` 添加幂等性保护 (`m_cleaningUpTaskIds`) | ProcessingController.cpp/h | P0 |
| 2 | 兜底定时器去重 (`m_orphanTimers`) | ProcessingController.cpp/h | P0 |
| 3 | `cancelMessageFileTasks` 添加去重保护 | ProcessingController.cpp | P1 |
| 4 | QML 删除按钮防抖（`_deleteInProgress` + Timer） | MediaThumbnailStrip.qml | P1 |
| 5 | QML 层改用 `removeMediaFileById`（fileId 替代索引） | MessageItem.qml | P2 |

## 验证方法

1. 启动程序，创建包含多个文件的处理任务（Shader 和 AI 各测试）
2. **快速连续点击**（≥5 次/秒）缩略图的 X 删除按钮
3. 验证场景：
   - [ ] 正在处理状态下的快速连击不崩溃
   - [ ] 等待状态下快速连击不崩溃
   - [ ] 失败状态下快速连击不崩溃
   - [ ] 只删除目标文件，不影响其他文件
   - [ ] 不产生定时器泄漏（观察内存稳定性）
   - [ ] 日志无 "cleanupTask already in progress" 以外的警告/错误
4. 构建运行确认无编译错误和运行时警告
