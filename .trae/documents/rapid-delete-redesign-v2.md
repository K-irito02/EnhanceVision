# 快速连续删除崩溃 — 重新设计方案 v2

> **目标**：确保用户连续快速点击 X 删除按钮时，应用程序稳定不崩溃，同时保持快速响应（≤200ms感知延迟）和良好的视觉反馈。

## 1. 根因分析（深度）

### 1.1 崩溃的精确触发路径

```
用户快速点击X (间隔 < 200ms)
  │
  ├─ QML: onClicked × N次
  │    └─ MessageItem.onDeleteFile → Qt.callLater(removeMediaFileById) × N
  │
  ├─ 事件循环迭代1:
  │    └─ removeMediaFileById #1 → removeMediaFile → QTimer::singleShot(0, lambda_1)
  │       → cancelMessageFileTasks(msgId, fileA)
  │          → cancelAndRemoveTask(taskA, ForceProcessing)
  │             → cleanupTask(taskA) ← 第1次进入
  │                ├── processor->cancel()        [设置原子取消标志]
  │                ├── m_cleaningUpTaskIds.insert(taskA)
  │                └── QTimer::singleShot(0, async_cleanup_A)  ← 入队
  │
  ├─ 事件循环迭代2:
  │    └─ removeMediaFileById #2 → removeMediaFile → QTimer::singleShot(0, lambda_2)
  │       → cancelMessageFileTasks(msgId, fileB)
  │          → cancelAndRemoveTask(taskB, ForceProcessing)
  │             → cleanupTask(taskB) ← 第2次进入
  │                └── QTimer::singleShot(0, async_cleanup_B)  ← 入队
  │
  ├─ [并发] AIVideoProcessor QtConcurrent线程:
  │    └─ processInternal检测到cancel标志 → 结束 → emitCompleted()
  │       └── completed信号 (QueuedConnection) → 入队 handler_completed_A
  │
  └─ 主线程事件循环执行（顺序不确定！）┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄  竞态区域
       │
       ├── 路径A: async_cleanup_A 先于 handler_completed_A 执行
       │    ├── disconnectAiEngineForTask(taskA) ✓
       │    ├── m_aiEnginePool->release(taskA)   ✓ 引擎释放#1
       │    └── m_activeAIVideoProcessors.contains(taskA)==true → 创建orphan timer
       │
       │    然后 handler_completed_A 执行：
       │    ├── m_activeAIVideoProcessors.contains(taskA) == true! (orphan timer未触发)
       │    ├── m_activeAIVideoProcessors.remove(taskA)
       │    ├── disconnectAiEngineForTask(taskA) → 空集，安全返回
       │    ├── m_tasks中taskA已被移除 → taskAlreadyRemoved=true ... 安全跳过?
       │    └── ⚠️ 但如果时序更复杂，taskA还在m_tasks中 → release(engine) → **DOUBLE RELEASE!**
       │
       └── 路径B: handler_completed_A 先于 async_cleanup_A 执行
            ├── m_activeAIVideoProcessors.contains(taskA) == true
            ├── m_tasks遍历：taskA可能还在！→ taskAlreadyRemoved=false
            ├── m_activeAIVideoProcessors.remove(taskA)
            ├── disconnectAiEngineForTask(taskA) → 断开连接
            ├── m_aiEnginePool->release(taskA)      ✓ 引擎释放#1
            ├── completeTask(taskId, result) 或 failTask(taskId, error)
            │    └── finalizeTask → cleanupTask(taskA) ← 第3次进入!
            │         └── m_cleaningUpTaskIds包含taskA → 幂等返回... 安全?
            │
            └── 然后async_cleanup_A执行：
                 ├── disconnectAiEngineForTask(taskA) → 已空，安全
                 ├── m_aiEnginePool->release(taskA)   → **DOUBLE RELEASE!** 💥
                 └── m_activeAIVideoProcessors.contains → false，不创建orphan
```

### 1.2 三大核心问题

| # | 问题 | 位置 | 后果 |
|---|------|------|------|
| P1 | **竞态条件**: `cleanupTask` 的 async lambda 与 `completed` 信号 handler 都操作同一组资源，无统一协调 | ProcessingController.cpp L1699 vs L1848-1875 | Double-release引擎 / 重复completeTask |
| P2 | **信号未屏蔽先清理资源**: cleanupTask 不立即断开 AIVideoProcessor 的 completed 信号连接，而是依赖后续 async lambda 来处理 | ProcessingController.cpp L1684-1689 vs L1699 | completed 信号仍可到达 |
| P3 | **QML debounce 过长 + 无视觉反馈**: 500ms+800ms=最长1300ms的无响应期，用户会反复点击加剧问题 | MediaThumbnailStrip.qml L418-422 + MessageItem.qml L130-135 | 用户以为没反应，疯狂连点 |

---

## 2. 设计原则

| 原则 | 说明 |
|------|------|
| **信号屏蔽优先** | 终止任务的第一步必须是断开信号连接，然后才清理资源 |
| **单一终结入口** | 所有终止路径(cancel/complete/fail/cleanup)必须汇聚到同一个方法 |
| **SharedPtr保活** | 从容器take出后保持sharedptr引用，防止对象在等待期间被销毁 |
| **短debounce + 视觉反馈** | QML层用200ms短debounce配合即时动画替代长debounce |
| **状态机替代碎片化QSet** | 用生命周期状态机替代m_cleaningUpTaskIds/m_pendingCancelKeys等分散的QSet |

---

## 3. 架构设计

### 3.1 任务生命周期状态机

```
              ┌─────────────┐
     创建任务  │   Active    │ ◄────────────────────┐
           ──►│  (正常运行)  │                      │
              └──────┬──────┘                      │
                     │ 取消请求                     │ 完成/失败
                     ▼                              ▼
              ┌─────────────┐                ┌────────────┐
              │  Canceling  │                │  Finishing  │
              │ (等待信号到达)│                │(完成/失败中) │
              └──────┬──────┘                └──────┬─────┘
                     │ 信号到达/超时                 │
                     ▼                              │
              ┌─────────────┐                       │
              │  Cleaning   │ ◄─────────────────────┘
              │ (清理资源中) │     所有路径汇聚到此处
              │  [互斥态]    │
              └──────┬──────┘
                     │ 清理完成
                     ▼
              ┌─────────────┐
              │    Dead      │
              │ (已完全清理)  │
              └─────────────┘
```

### 3.2 数据流（修复后的完整流程）

```
QML Click → [200ms debounce] → Qt.callLater(300ms message-lock)
  → MessageModel.removeMediaFileById(msgId, fileId)
    → MessageModel.removeMediaFile (fileId重查找)
      → [m_removingMessageIds序列化]
      → QTimer::singleShot(0):
        → ProcessingController.cancelMessageFileTasks(msgId, fileId)
          → [m_pendingCancelKeys去重]
          → cancelAndRemoveTask → terminateTask(taskId)  ★ 唯一入口
            ├── 阶段1(同步): 断开信号 + cancel() + take出processor
            ├── 阶段2(异步): waitForFinished + release engine + 清理maps
            └── 阶段3(异步): 从m_tasks移除 + 更新UI
        → MessageModel实际删除文件 + emit signals
```

---

## 4. 实施方案

### 4.0 文件变更清单

| 文件 | 变更类型 | 说明 |
|------|----------|------|
| `include/EnhanceVision/controllers/ProcessingController.h` | 修改 | 新增枚举、状态map、terminateTask声明；精简冗余成员变量 |
| `src/controllers/ProcessingController.cpp` | **重写核心** | 新增terminateTask()；重构cleanupTask()为委托；重构launchAIVideoProcessor的completed handler |
| `include/EnhanceVision/models/MessageModel.h` | 微调 | 保留m_removingMessageIds |
| `src/models/MessageModel.cpp` | 优化 | 保留现有逻辑，微调注释 |
| `qml/components/MediaThumbnailStrip.qml` | 修改 | 缩短debounce至200ms + 添加删除动画 |
| `qml/components/MessageItem.qml` | 修改 | 缩短message-lock至300ms + 添加即时视觉反馈 |

### 4.1 Layer 5&6 核心：ProcessingController 重构

#### 4.1.1 新增生命周期枚举和状态跟踪

**ProcessingController.h** — 新增：

```cpp
enum class TaskLifecycle {
    Active,      // 正常运行
    Canceling,   // 已请求取消，等待异步清理
    Cleaning,    // 正在清理资源（互斥）
    Dead         // 已完全清理
};

// 替代 m_cleaningUpTaskIds + m_pendingCancelKeys 的碎片化方案
QHash<QString, TaskLifecycle> m_taskLifecycle;  // taskId → 当前生命周期状态
QHash<QString, QSharedPointer<AIVideoProcessor>> m_dyingProcessors;  // 等待结束的processor(保活)
```

#### 4.1.2 新增 `terminateTask()` — 统一终结入口

**ProcessingController.cpp** — 核心新方法：

```cpp
void ProcessingController::terminateTask(const QString& taskId, const QString& reason)
{
    TaskLifecycle state = m_taskLifecycle.value(taskId, TaskLifecycle::Active);

    if (state == TaskLifecycle::Cleaning || state == TaskLifecycle::Dead) {
        return;  // 已在清理或已死，幂等返回
    }

    if (state == TaskLifecycle::Canceling) {
        return;  // 已在等待取消，无需重复
    }

    m_taskLifecycle[taskId] = TaskLifecycle::Canceling;

    qInfo() << "[ProcessingController] terminateTask:" << taskId << "reason:" << reason;

    // === 阶段1: 同步 — 屏蔽信号 + 发送取消请求 ===

    // 1a. 立即断开 AI 引擎信号（关键！防止后续completed信号到达）
    disconnectAiEngineForTask(taskId);

    // 1b. 取消视频处理器（仅设置原子取消标志，不阻塞）
    if (m_activeAIVideoProcessors.contains(taskId)) {
        auto processor = m_activeAIVideoProcessors.take(taskId);  // take出来！
        if (processor) {
            processor->cancel();  // 设置取消标志
            m_dyingProcessors[taskId] = processor;  // 保持引用，防止销毁
        }
    }

    // 1c. 取消Shader视频处理器
    cancelVideoProcessing(taskId);

    // 1d. 取消图片处理器
    QSharedPointer<ImageProcessor> imgProcessor;
    if (m_activeImageProcessors.contains(taskId)) {
        imgProcessor = m_activeImageProcessors.take(taskId);
        if (imgProcessor) { imgProcessor->cancel(); }
    }

    // 1e. 注销任务状态
    TaskStateManager::instance()->unregisterActiveTask(taskId);

    // === 阶段2: 异步 — 等待线程结束 + 释放资源 ===
    m_taskLifecycle[taskId] = TaskLifecycle::Cleaning;

    QTimer::singleShot(0, this, [this, taskId]() {
        // 2a. 等待AI视频处理器线程结束（最多3秒）
        if (m_dyingProcessors.contains(taskId)) {
            auto processor = m_dyingProcessors.take(taskId);
            if (processor) {
                processor->waitForFinished(3000);
            }
        }

        // 2b. 释放引擎回池（只释放一次！）
        m_aiEnginePool->release(taskId);

        // 2c. 清理所有关联资源
        m_resourceManager->release(taskId);
        m_taskCoordinator->unregisterTask(taskId);
        m_pendingExports.remove(taskId);
        m_taskContexts.remove(taskId);
        m_lastReportedTaskProgress.remove(taskId);
        m_lastTaskProgressUpdateMs.remove(taskId);
        m_taskMessages.remove(taskId);
        m_pendingModelLoadTaskIds.remove(taskId);

        // 2d. 清理旧的orphan timers（兼容性保留但基本不会用到）
        if (m_orphanTimers.contains(taskId)) {
            QTimer* oldTimer = m_orphanTimers.take(taskId);
            if (oldTimer) { oldTimer->stop(); oldTimer->deleteLater(); }
        }

        // 2e. 标记为Dead
        m_taskLifecycle[taskId] = TaskLifecycle::Dead;

        qInfo() << "[ProcessingController] terminateTask completed:" << taskId;
    });
}
```

#### 4.1.3 重构 `cleanupTask()` 为委托

```cpp
void ProcessingController::cleanupTask(const QString& taskId)
{
    terminateTask(taskId, "cleanupTask");
}
```

#### 4.1.4 重构 `launchAIVideoProcessor` 的 completed handler

```cpp
connect(processor.data(), &AIVideoProcessor::completed,
    this, [this, taskId](bool success, const QString& result, const QString& error) {
        // 检查1: 生命周期检查（如果已被terminateTask接管，直接忽略）
        TaskLifecycle lifecycle = m_taskLifecycle.value(taskId, TaskLifecycle::Active);
        if (lifecycle == TaskLifecycle::Canceling ||
            lifecycle == TaskLifecycle::Cleaning ||
            lifecycle == TaskLifecycle::Dead) {
            qInfo() << "[ProcessingController][AI] completed signal ignored for"
                    << taskId << "lifecycle:" << static_cast<int>(lifecycle);
            return;
        }

        // 检查2: 容器检查（双重保险）
        if (!m_activeAIVideoProcessors.contains(taskId)) {
            return;
        }

        bool taskExists = false;
        for (const auto& t : m_tasks) {
            if (t.taskId == taskId) { taskExists = true; break; }
        }

        // 从容器移除
        m_activeAIVideoProcessors.remove(taskId);
        disconnectAiEngineForTask(taskId);

        if (taskExists) {
            m_aiEnginePool->release(taskId);  // 正常路径：唯一一次release
            if (success) {
                completeTask(taskId, result);
            } else {
                failTask(taskId, error);
            }
        } else {
            m_aiEnginePool->release(taskId);  // 任务已被外部移除但仍需释放引擎
        }
    }, Qt::QueuedConnection);
```

#### 4.1.5 更新 `cancelAndRemoveTask` 调用 terminateTask

```cpp
ProcessingController::CancelResult ProcessingController::cancelAndRemoveTask(int index, CancelMode mode)
{
    CancelResult result;
    if (index < 0 || index >= m_tasks.size()) { return result; }

    const QString taskId = m_tasks[index].taskId;
    const ProcessingStatus oldStatus = m_tasks[index].status;

    if (oldStatus == ProcessingStatus::Pending) {
        m_tasks[index].status = ProcessingStatus::Cancelled;
        terminateTask(taskId, "cancelAndRemoveTask-Pending");  // 改用terminateTask
        emit taskCancelled(taskId);
        m_tasks.removeAt(index);
        result.cancelledPending = 1;
    } else if (oldStatus == ProcessingStatus::Processing && mode == CancelMode::ForceProcessing) {
        m_taskCoordinator->requestCancellation(taskId);
        terminateTask(taskId, "cancelAndRemoveTask-Processing");  // 改用terminateTask
        m_tasks[index].status = ProcessingStatus::Cancelled;
        emit taskCancelled(taskId);
        m_tasks.removeAt(index);
        result.cancelledProcessing = 1;
    }

    return result;
}
```

### 4.2 Layer 3: MessageModel 优化

**MessageModel.cpp** — `removeMediaFile()` 保持现有逻辑不变，已经足够健壮：

- ✅ `m_removingMessageIds` 序列化保留
- ✅ fileId 重查找机制保留
- ✅ 异步 lambda 内重新查找 messageIndex 保留

无需修改。

### 4.3 Layer 1&2: QML 层优化

#### 4.3.1 MediaThumbnailStrip.qml — 缩短debounce + 删除动画

**两个 delegate 共同修改** (`horizontalComponent` delegate 和 `gridComponent` delegate):

```qml
// 修改前:
property bool _deleteInProgress: false
Timer {
    id: debounceTimer  // gridDebounceTimer
    interval: 500      // ← 太长！
    repeat: false
    onTriggered: thumbDelegate._deleteInProgress = false
}

// 修改后:
property bool _deleteInProgress: false
property real _deleteOpacity: 1.0  // 新增：删除淡出效果

Behavior on _deleteOpacity {
    NumberAnimation { duration: 200; easing.type: Easing.InOutCubic }
}

Timer {
    id: debounceTimer  // gridDebounceTimer
    interval: 200      // ← 缩短到200ms
    repeat: false
    onTriggered: thumbDelegate._deleteInProgress = false
}

// 在 Rectangle{ id: thumbRect } 上新增:
opacity: thumbDelegate._deleteOpacity
```

**4个 MouseArea 的 onClicked 统一改为**:

```qml
onClicked: {
    thumbDelegate._deleteInProgress = true
    thumbDelegate._deleteOpacity = 0.3  // 即时视觉反馈：变半透明
    debounceTimer.start()
    var origIndex = thumbDelegate.itemData ? thumbDelegate.itemData.origIndex : thumbDelegate.index
    root.deleteFile(origIndex)
}
```

同时将 deleteBtn MouseArea 的 `enabled` 改为始终 true（用透明度表达禁用状态）:

```qml
MouseArea {
    id: deleteBtnMouse
    anchors.fill: parent
    hoverEnabled: true
    cursorShape: thumbDelegate._deleteInProgress ? Qt.ArrowCursor : Qt.PointingHandCursor
    // 移除 enabled: !thumbDelegate._deleteInProgress
    // 改为内部拦截:
    onClicked: {
        if (thumbDelegate._deleteInProgress) return  // 内部静默忽略
        thumbDelegate._deleteInProgress = true
        thumbDelegate._deleteOpacity = 0.3
        debounceTimer.start()
        var origIndex = thumbDelegate.itemData ? thumbDelegate.itemData.origIndex : thumbDelegate.index
        root.deleteFile(origIndex)
    }
}
```

`deleteBtnForFailedMouse` 同理修改。

**文件删除完成后恢复透明度**：通过 Connections 监听 filteredModel 变化来恢复（或简单地在debounce结束时恢复）:

```qml
onTriggered: {
    thumbDelegate._deleteInProgress = false
    thumbDelegate._deleteOpacity = 1.0  // 恢复
}
```

#### 4.3.2 MessageItem.qml — 缩短消息锁 + 视觉反馈

```qml
// 修改前:
property bool _messageDeleteLocked: false
Timer {
    id: messageDeleteDebounceTimer
    interval: 800  // ← 太长！
    ...
}

// 修改后:
property bool _messageDeleteLocked: false
property real _deletingOverlayOpacity: 0  // 新增：全卡片遮罩效果

Timer {
    id: messageDeleteDebounceTimer
    interval: 300  // ← 缩短到300ms
    repeat: false
    onTriggered: {
        root._messageDeleteLocked = false
        root._deletingOverlayOpacity = 0  // 恢复
    }
}

// onDeleteFile 修改:
onDeleteFile: function(index) {
    if (root._messageDeleteLocked) return
    root._messageDeleteLocked = true
    root._deletingOverlayOpacity = 1  // 显示遮罩
    messageDeleteDebounceTimer.start()
    var _msgId = root.taskId
    var _fileId = ""
    if (root.mediaFiles && index >= 0 && index < root.mediaFiles.count) {
        var _item = root.mediaFiles.get(index)
        if (_item) _fileId = _item.id
    }
    var __msgId = _msgId
    var __fileId = _fileId
    Qt.callLater(function() {
        if (__fileId) {
            messageModel.removeMediaFileById(__msgId, __fileId)
        }
    })
}

// 在根 Rectangle 内部最顶层新增删除遮罩:
Rectangle {
    anchors.fill: parent
    color: Theme.colors.card
    opacity: root._deletingOverlayOpacity * 0.3
    visible: root._deletingOverlayOpacity > 0
    z: 999  // 最顶层

    Behavior on opacity {
        NumberAnimation { duration: 150 }
    }

    ColoredIcon {
        anchors.centerIn: parent
        source: Theme.icon("loader")
        iconSize: 20
        color: Theme.colors.mutedForeground
        opacity: 0.6
        RotationAnimation on rotation {
            from: 0; to: 360
            duration: 800
            loops: Animation.Infinite
            running: root._deletingOverlayOpacity > 0
        }
    }
}
```

---

## 5. 实施步骤

### Step 1: ProcessingController.h — 新增类型和成员变量
- 新增 `TaskLifecycle` 枚举
- 新增 `m_taskLifecycle` (QHash<QString, TaskLifecycle>)
- 新增 `m_dyingProcessors` (QHash<QString, QSharedPointer<AIVideoProcessor>>)
- 保留 `m_orphanTimers` 和 `m_pendingCancelKeys`（向后兼容）

### Step 2: ProcessingController.cpp — 实现 terminateTask()
- 编写完整的 terminateTask 方法（阶段1同步 + 阶段2异步）
- 重构 cleanupTask() 为 terminateTask 的简单委托
- 更新 cancelAndRemoveTask() 调用 terminateTask()

### Step 3: ProcessingController.cpp — 修复 launchAIVideoProcessor completed handler
- 添加生命周期状态检查
- 确保 engine 只释放一次
- 添加详细日志便于调试

### Step 4: ProcessingController.cpp — 更新 handleOrphanedVideoTask
- 兼容新的生命周期状态
- 使用 terminateTask 的逻辑

### Step 5: QML MediaThumbnailStrip — 缩短debounce + 删除动画
- interval: 500→200
- 新增 `_deleteOpacity` 淡出效果
- 删除按钮点击即时视觉反馈

### Step 6: QML MessageItem — 缩短消息锁 + 全卡遮罩
- interval: 800→300
- 新增删除遮罩 + loading spinner

### Step 7: 构建验证
- 编译通过
- 运行测试
- 日志检查

---

## 6. 测试验证矩阵

| 场景 | 操作 | 预期结果 |
|------|------|----------|
| 单文件删除(processing) | 点击X一次 | 文件淡出消失，任务正常取消 |
| 单文件删除(pending) | 点击X一次 | 文件立即消失 |
| 单文件删除(failed) | 点击X一次 | 文件立即消失 |
| 快速连删同文件(×10次) | 200ms内狂点同一X | 只有第1次生效，其余被debounce拦截，无崩溃 |
| 快速连删不同文件(processing+pending) | 交替点击两个文件的X | 两个文件都成功删除，无崩溃 |
| 快速连删导致消息自动移除 | 删除消息中最后一个文件 | 消息整体消失，无崩溃 |
| AI推理视频中连删 | AI视频处理中快速点击X | 处理器正常取消，引擎正确释放，无闪退 |
| Shader模式连删 | Shader处理中快速点击X | 正常取消，无崩溃 |
| 删除后立即添加新任务 | 删除后立刻添加新任务 | 新任务正常启动，引擎可复用 |
