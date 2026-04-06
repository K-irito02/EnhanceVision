# 暂停/继续按钮功能修复计划

## 问题分析

### 问题1：消息卡片没有显示继续按钮

**根本原因**：
- 暂停图标文件缺失：`qrc:/icons/pause-circle.svg` 不存在
- 日志显示：`[WARN] qrc:/qt/qml/EnhanceVision/qml/controls/ColoredIcon.qml:17:5: QML QQuickImage: Cannot open: qrc:/icons/pause-circle.svg`

**影响**：
- 设置页面中暂停模式选择卡片无法正常显示暂停图标
- 可能影响其他使用 `pause-circle` 图标的组件

### 问题2：暂停状态显示不完整

**根本原因**：
- 缩略图上只显示暂停图标，没有文字说明
- MediaThumbnailStrip.qml 中暂停状态的视觉反馈不够清晰

**当前实现**：
```qml
// MediaThumbnailStrip.qml:592
source: thumbDelegate.isPaused ? Theme.icon("pause") : Theme.icon("loader")
```

**问题**：
- 只显示图标，没有文字说明当前状态
- 用户可能不清楚这是暂停状态还是其他状态

### 问题3：队列不能继续处理其他消息

**根本原因**：
- AI 引擎取消超时，导致引擎池无法正确释放
- 日志显示：
  ```
  [2026-04-06 14:44:07.510] [INFO] [AIEngine] cancelProcess() called, isProcessing: true
  [2026-04-06 14:44:09.531] [WARN] [AIEnginePool] Engine still processing after timeout
  ```
- 引擎在 2 秒超时后仍未停止处理，导致 `availableCount()` 返回 0

**代码分析**：
```cpp
// AIEnginePool.cpp:185-191
while (engine->isProcessing() && waitTimer.elapsed() < 2000) {
    QThread::msleep(50);
}
if (engine->isProcessing()) {
    qWarning() << "[AIEnginePool] Engine still processing after timeout";
}
```

**影响**：
- 引擎状态未正确设置为 `Ready`
- `availableCount()` 检查引擎是否真正空闲（不在处理中）
- 导致后续任务无法获取引擎，队列阻塞

## 修复方案

### 修复1：添加缺失的暂停图标文件

**步骤**：
1. 检查项目中是否存在 `pause-circle.svg` 文件
2. 如果不存在，创建该图标文件
3. 确保图标文件已添加到资源文件（.qrc）中
4. 验证图标在设置页面中正确显示

**文件位置**：
- 图标文件：`resources/icons/pause-circle.svg`
- 资源文件：`resources/resources.qrc`

### 修复2：增强暂停状态的视觉反馈

**改进方案**：
1. 在缩略图上添加暂停状态的文字标签
2. 使用更明显的视觉提示（如半透明遮罩层）
3. 显示"已暂停"文字说明

**实现位置**：
- 文件：`qml/components/MediaThumbnailStrip.qml`
- 修改缩略图暂停状态的显示逻辑

**示例代码**：
```qml
// 在缩略图上添加暂停状态遮罩层
Rectangle {
    anchors.fill: parent
    color: Qt.rgba(0, 0, 0, 0.5)
    visible: thumbDelegate.isPaused
    radius: Theme.radius.md
    
    Column {
        anchors.centerIn: parent
        spacing: 4
        
        ColoredIcon {
            source: Theme.icon("pause")
            iconSize: 24
            color: Theme.colors.warning
        }
        
        Text {
            text: qsTr("已暂停")
            color: Theme.colors.warning
            font.pixelSize: 12
            font.bold: true
        }
    }
}
```

### 修复3：改进 AI 引擎取消机制

**问题分析**：
- 当前实现使用同步等待（最多 2 秒）
- 如果引擎在 CPU 模式下处理大图，取消操作可能需要更长时间
- 超时后引擎状态未正确更新，导致队列阻塞

**改进方案**：

#### 方案 A：异步取消机制（推荐）

**优点**：
- 不阻塞主线程
- 引擎可以在后台完成取消操作
- 队列可以立即继续处理其他任务

**实现步骤**：
1. 修改 `AIEnginePool::release()` 方法，使用异步取消
2. 引擎取消完成后，通过信号通知引擎池
3. 引擎池更新状态并触发 `processNextTask`

**示例代码**：
```cpp
void AIEnginePool::release(const QString& taskId)
{
    QMutexLocker locker(&m_mutex);

    if (!m_taskToSlot.contains(taskId)) {
        return;
    }

    int idx = m_taskToSlot.take(taskId);
    if (idx >= 0 && idx < m_slots.size()) {
        AIEngine* engine = m_slots[idx].engine;
        
        if (engine && engine->isProcessing()) {
            qInfo() << "[AIEnginePool] Async cancelling engine for task:" << taskId;
            
            // 标记引擎为 Draining 状态（正在释放）
            m_slots[idx].state = EngineState::Draining;
            
            // 异步取消引擎
            connect(engine, &AIEngine::processingCancelled, this, [this, idx, taskId]() {
                QMutexLocker locker(&m_mutex);
                m_slots[idx].state = EngineState::Ready;
                m_slots[idx].taskId.clear();
                m_slots[idx].backendType = BackendType::NCNN_CPU;
                m_slots[idx].wasUsed = true;
                
                qInfo() << "[AIEnginePool] Engine async released, task:" << taskId;
                emit engineReleased(taskId, idx);
                
                // 触发处理下一个任务
                QMetaObject::invokeMethod(this, "engineReady", Qt::QueuedConnection);
            }, Qt::QueuedConnection);
            
            engine->cancelProcess();
        } else {
            // 引擎未在处理，直接释放
            m_slots[idx].state = EngineState::Ready;
            m_slots[idx].taskId.clear();
            m_slots[idx].backendType = BackendType::NCNN_CPU;
            m_slots[idx].wasUsed = true;
            
            emit engineReleased(taskId, idx);
        }
    }
}
```

#### 方案 B：增加超时时间并强制重置

**优点**：
- 实现简单
- 确保引擎最终被释放

**缺点**：
- 可能阻塞主线程更长时间
- 强制重置可能导致资源泄漏

**实现步骤**：
1. 增加取消超时时间（从 2 秒增加到 5 秒）
2. 超时后强制重置引擎状态
3. 更新引擎池状态

**示例代码**：
```cpp
void AIEnginePool::release(const QString& taskId)
{
    QMutexLocker locker(&m_mutex);

    if (!m_taskToSlot.contains(taskId)) {
        return;
    }

    int idx = m_taskToSlot.take(taskId);
    if (idx >= 0 && idx < m_slots.size()) {
        AIEngine* engine = m_slots[idx].engine;
        
        if (engine && engine->isProcessing()) {
            qInfo() << "[AIEnginePool] Cancelling engine before release, task:" << taskId;
            engine->cancelProcess();
            
            // 增加超时时间到 5 秒
            locker.unlock();
            QElapsedTimer waitTimer;
            waitTimer.start();
            while (engine->isProcessing() && waitTimer.elapsed() < 5000) {
                QThread::msleep(50);
            }
            locker.relock();
            
            if (engine->isProcessing()) {
                qWarning() << "[AIEnginePool] Engine still processing after timeout, forcing reset";
                // 强制重置引擎状态
                engine->resetState();
            } else {
                qInfo() << "[AIEnginePool] Engine stopped after" << waitTimer.elapsed() << "ms";
            }
        }
        
        m_slots[idx].state = EngineState::Ready;
        m_slots[idx].taskId.clear();
        m_slots[idx].backendType = BackendType::NCNN_CPU;
        m_slots[idx].wasUsed = true;

        qInfo() << "[AIEnginePool] Task released:" << taskId << ", slot index:" << idx;
        emit engineReleased(taskId, idx);
    }
}
```

#### 方案 C：立即释放引擎，不等待取消完成

**优点**：
- 队列可以立即继续处理
- 不阻塞主线程

**缺点**：
- 引擎可能在后台继续运行一段时间
- 需要确保引擎能够安全地完成取消操作

**实现步骤**：
1. 修改 `availableCount()` 逻辑，排除正在取消的引擎
2. 引擎取消完成后自动更新状态
3. 添加引擎状态：`EngineState::Cancelling`

**示例代码**：
```cpp
enum class EngineState {
    Uninitialized,
    Ready,
    InUse,
    Cancelling,  // 新增：正在取消
    Draining     // 正在释放
};

void AIEnginePool::release(const QString& taskId)
{
    QMutexLocker locker(&m_mutex);

    if (!m_taskToSlot.contains(taskId)) {
        return;
    }

    int idx = m_taskToSlot.take(taskId);
    if (idx >= 0 && idx < m_slots.size()) {
        AIEngine* engine = m_slots[idx].engine;
        
        if (engine && engine->isProcessing()) {
            qInfo() << "[AIEnginePool] Marking engine as Cancelling, task:" << taskId;
            
            // 标记为正在取消
            m_slots[idx].state = EngineState::Cancelling;
            
            // 异步取消引擎
            connect(engine, &AIEngine::processingCancelled, this, [this, idx]() {
                QMutexLocker locker(&m_mutex);
                m_slots[idx].state = EngineState::Ready;
                m_slots[idx].taskId.clear();
                qInfo() << "[AIEnginePool] Engine cancellation completed";
            }, Qt::QueuedConnection);
            
            engine->cancelProcess();
        } else {
            m_slots[idx].state = EngineState::Ready;
            m_slots[idx].taskId.clear();
        }
        
        m_slots[idx].backendType = BackendType::NCNN_CPU;
        m_slots[idx].wasUsed = true;
        
        emit engineReleased(taskId, idx);
    }
}

int AIEnginePool::availableCount() const
{
    int count = 0;
    for (const auto& slot : m_slots) {
        // 排除正在取消的引擎
        if (slot.state == EngineState::Ready || slot.state == EngineState::Uninitialized) {
            if (slot.engine && !slot.engine->isProcessing()) {
                count++;
            } else if (!slot.engine) {
                count++;
            }
        }
    }
    return count;
}
```

**推荐方案**：方案 A（异步取消机制）

**理由**：
1. 不阻塞主线程，用户体验更好
2. 引擎可以在后台安全完成取消操作
3. 队列可以立即继续处理其他任务
4. 代码结构清晰，易于维护

## 实施步骤

### 第一阶段：修复图标文件（优先级：高）

1. 检查并创建 `pause-circle.svg` 图标文件
2. 添加到资源文件
3. 验证设置页面中图标显示正常

### 第二阶段：增强暂停状态显示（优先级：中）

1. 修改 `MediaThumbnailStrip.qml`，添加暂停状态文字标签
2. 优化暂停状态的视觉反馈
3. 测试暂停状态的显示效果

### 第三阶段：改进引擎取消机制（优先级：高）

1. 实现异步取消机制（方案 A）
2. 修改 `AIEnginePool` 和 `AIEngine` 类
3. 添加必要的信号和状态管理
4. 测试队列继续处理功能

## 测试计划

### 测试1：图标显示测试

**步骤**：
1. 打开设置页面
2. 查看暂停模式选择卡片
3. 验证暂停图标正常显示

**预期结果**：
- 暂停图标正常显示
- 无警告信息

### 测试2：暂停状态显示测试

**步骤**：
1. 在 CPU 模式下处理图片
2. 点击暂停按钮
3. 查看缩略图上的暂停状态显示

**预期结果**：
- 缩略图上显示暂停图标和"已暂停"文字
- 视觉反馈清晰明了

### 测试3：队列继续处理测试

**步骤**：
1. 添加多个处理任务到队列
2. 在第一个任务处理时点击暂停
3. 观察队列是否继续处理其他任务

**预期结果**：
- 第一个任务暂停后，队列继续处理第二个任务
- 日志显示引擎正确释放
- `availableCount()` 返回正确值

### 测试4：继续处理测试

**步骤**：
1. 在暂停状态下点击继续按钮
2. 观察任务是否继续处理

**预期结果**：
- 任务从暂停状态恢复
- 继续按钮正确显示
- 处理正常完成

## 风险评估

### 风险1：异步取消可能导致资源竞争

**缓解措施**：
- 使用互斥锁保护共享状态
- 添加状态一致性检查
- 实现超时保护机制

### 风险2：图标文件格式不兼容

**缓解措施**：
- 使用标准 SVG 格式
- 测试不同主题下的显示效果
- 提供备用图标方案

### 风险3：暂停状态显示影响性能

**缓解措施**：
- 使用轻量级的 QML 组件
- 避免复杂的动画效果
- 进行性能测试

## 后续优化建议

1. **添加暂停/继续的动画效果**：提升用户体验
2. **优化引擎取消算法**：减少取消等待时间
3. **增强错误处理**：提供更友好的错误提示
4. **添加暂停历史记录**：方便用户查看操作历史

## 总结

本计划针对"暂停/继续"按钮功能的三个主要问题提出了详细的修复方案。通过添加缺失的图标文件、增强暂停状态的视觉反馈、改进 AI 引擎的取消机制，可以全面提升用户体验和系统稳定性。推荐采用异步取消机制（方案 A）来解决队列阻塞问题，确保系统在暂停操作后能够正常继续处理其他任务。
