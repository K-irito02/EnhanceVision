# AI推理模块崩溃问题诊断与解决方案

## 问题概述

针对AI推理模式中出现的三个关键崩溃问题，本计划提供系统性分析和彻底解决方案。

---

## 问题一：首次处理任务间歇性崩溃

### 症状描述
应用程序运行后首次处理任务时存在间歇性崩溃闪退问题，表现为任务处理时而成功时而失败并导致程序异常退出。

### 根本原因分析

#### 1. Vulkan 初始化竞争条件
**位置**: `AIEngine.cpp`

```cpp
// runInference() 中的临时解决方案
static std::atomic<bool> s_vulkanWarmedUp{false};
if (!s_vulkanWarmedUp.load() && m_opt.use_vulkan_compute) {
    QThread::msleep(200);  // 不可靠的等待
    s_vulkanWarmedUp.store(true);
}
```

**问题**:
- 使用静态变量和固定等待时间不可靠
- Vulkan 管线编译是异步的，200ms 可能不够
- 多引擎实例共享静态变量导致状态混乱

#### 2. 引擎池首次获取问题
**位置**: `AIEnginePool.cpp`

```cpp
AIEngine* AIEnginePool::acquire(const QString& taskId) {
    // ...
    if (m_slots[i].engine && m_slots[i].wasUsed) {
        m_slots[i].engine->forceCancel();
        m_slots[i].engine->cleanupGpuMemory();
        m_slots[i].engine->resetState();
    }
    m_slots[i].wasUsed = true;
    // ...
}
```

**问题**:
- 首次获取时跳过清理，但引擎可能处于未定义状态
- Vulkan allocator 可能未正确初始化
- 没有确保引擎完全就绪的机制

#### 3. 模型加载异步问题
**位置**: `ProcessingController.cpp`

```cpp
void ProcessingController::startTask(QueueTask& task) {
    // ...
    if (engine->currentModelId() == modelId) {
        launchAiInference(engine, taskId, inputPath, outputPath, message);
    } else {
        m_pendingModelLoadTaskIds.insert(taskId);
        QMetaObject::Connection loadConn = connect(engine, &AIEngine::modelLoadCompleted,
            this, [...] (bool success, ...) {
                // 模型加载完成后启动推理
            }, Qt::SingleShotConnection);
        engine->loadModelAsync(modelId);
    }
}
```

**问题**:
- 模型加载失败时任务状态可能不一致
- 异步加载期间任务可能被取消但未正确处理
- 信号连接使用 SingleShotConnection，可能遗漏错误处理

#### 4. 线程安全问题
**位置**: `AIEngine.cpp`

```cpp
void AIEngine::processAsync(const QString &inputPath, const QString &outputPath) {
    QtConcurrent::run([this, inputPath, outputPath, snapshotParams]() {
        // ...
        QImage result = process(inputImage);  // 在工作线程调用
        // ...
        emit processFileCompleted(true, outputPath, QString());  // 信号发射
    });
}
```

**问题**:
- `process()` 内部发射信号，可能在工作线程触发 UI 更新
- `QMetaObject::invokeMethod` 使用 `Qt::QueuedConnection`，但错误处理可能不同步

### 解决方案

#### 方案 1.1: Vulkan 初始化同步机制重构

**修改文件**: `AIEngine.cpp`

```cpp
// 新增成员变量
std::atomic<bool> m_vulkanReady{false};
std::mutex m_vulkanInitMutex;
std::condition_variable m_vulkanInitCv;

// 重构 initVulkan()
void AIEngine::initVulkan() {
#if NCNN_VULKAN
    // 在专用线程中初始化 Vulkan
    QtConcurrent::run([this]() {
        int retryCount = 0;
        const int maxRetries = 5;
        
        while (retryCount < maxRetries && !m_gpuAvailable) {
            ncnn::create_gpu_instance();
            
            if (ncnn::get_gpu_count() > 0) {
                int gpuIndex = ncnn::get_default_gpu_index();
                m_vkdev = ncnn::get_gpu_device(gpuIndex);
                
                if (m_vkdev) {
                    // 预热 Vulkan 管线
                    warmupVulkanPipeline();
                    m_gpuAvailable = true;
                }
            }
            
            if (!m_gpuAvailable) {
                ncnn::destroy_gpu_instance();
                QThread::msleep(100);
                retryCount++;
            }
        }
        
        // 通知初始化完成
        m_vulkanReady.store(true);
        m_vulkanInitCv.notify_all();
        
        emit gpuAvailableChanged(m_gpuAvailable);
    });
#endif
}

// 新增 Vulkan 管线预热方法
void AIEngine::warmupVulkanPipeline() {
    if (!m_vkdev) return;
    
    // 创建一个小的测试推理来预热 Vulkan shader 编译
    ncnn::Net testNet;
    testNet.opt = m_opt;
    
    // 使用 1x1 输入进行预热
    ncnn::Mat dummy(1, 1, 3);
    // ... 执行最小化推理
}

// 重构 ensureVulkanReady()
void AIEngine::ensureVulkanReady() {
    if (!m_gpuAvailable || !m_useGpu) return;
    
    // 等待 Vulkan 初始化完成
    std::unique_lock<std::mutex> lock(m_vulkanInitMutex);
    m_vulkanInitCv.wait(lock, [this]() {
        return m_vulkanReady.load();
    });
    
    // 确保 allocator 已就绪
    QMutexLocker locker(&m_mutex);
    if (!m_blobVkAllocator || !m_stagingVkAllocator) {
        m_blobVkAllocator = m_vkdev->acquire_blob_allocator();
        m_stagingVkAllocator = m_vkdev->acquire_staging_allocator();
        m_opt.blob_vkallocator = m_blobVkAllocator;
        m_opt.workspace_vkallocator = m_stagingVkAllocator;
    }
}
```

#### 方案 1.2: 引擎池状态管理增强

**修改文件**: `AIEnginePool.cpp`

```cpp
// 新增引擎状态枚举
enum class EngineState {
    Uninitialized,
    Ready,
    InUse,
    Error
};

struct EngineSlot {
    AIEngine* engine = nullptr;
    QString taskId;
    EngineState state = EngineState::Uninitialized;
    QString lastError;
};

AIEngine* AIEnginePool::acquire(const QString& taskId) {
    QMutexLocker locker(&m_mutex);
    
    for (int i = 0; i < m_slots.size(); ++i) {
        if (m_slots[i].state == EngineState::Ready || 
            m_slots[i].state == EngineState::Uninitialized) {
            
            // 确保引擎完全就绪
            if (!ensureEngineReady(i)) {
                continue;  // 跳过不可用的引擎
            }
            
            m_slots[i].state = EngineState::InUse;
            m_slots[i].taskId = taskId;
            m_taskToSlot[taskId] = i;
            
            emit engineAcquired(taskId, i);
            return m_slots[i].engine;
        }
    }
    
    emit poolExhausted();
    return nullptr;
}

bool AIEnginePool::ensureEngineReady(int slotIndex) {
    auto& slot = m_slots[slotIndex];
    
    if (!slot.engine) {
        return false;
    }
    
    // 检查引擎健康状态
    if (slot.state == EngineState::Error) {
        // 尝试恢复引擎
        slot.engine->resetState();
        slot.engine->cleanupGpuMemory();
    }
    
    // 确保 Vulkan 就绪
    slot.engine->ensureVulkanReady();
    
    // 验证引擎可用性
    if (slot.engine->gpuAvailable() || !slot.engine->useGpu()) {
        slot.state = EngineState::Ready;
        return true;
    }
    
    slot.state = EngineState::Error;
    return false;
}
```

#### 方案 1.3: 模型加载同步机制

**修改文件**: `ProcessingController.cpp`

```cpp
void ProcessingController::startTask(QueueTask& task) {
    // ...
    
    if (message.mode == ProcessingMode::AIInference) {
        QString modelId = message.aiParams.modelId;
        
        AIEngine* engine = m_aiEnginePool->acquire(taskId);
        if (!engine) {
            failTask(taskId, tr("AI引擎池已耗尽，无法启动任务"));
            return;
        }
        
        // 同步模型加载（带超时）
        if (engine->currentModelId() != modelId) {
            QElapsedTimer loadTimer;
            loadTimer.start();
            const int kMaxLoadTimeMs = 30000;  // 30秒超时
            
            // 使用同步加载，但带进度回调
            bool loadSuccess = false;
            QString loadError;
            
            QEventLoop loadLoop;
            connect(engine, &AIEngine::modelLoadCompleted, 
                    [&](bool success, const QString& mid, const QString& error) {
                loadSuccess = success;
                loadError = error;
                loadLoop.quit();
            });
            
            engine->loadModelAsync(modelId);
            
            // 等待加载完成或超时
            if (loadTimer.elapsed() < kMaxLoadTimeMs) {
                loadLoop.exec();
            }
            
            if (!loadSuccess) {
                m_aiEnginePool->release(taskId);
                failTask(taskId, loadError.isEmpty() ? tr("模型加载失败") : loadError);
                return;
            }
        }
        
        connectAiEngineForTask(engine, taskId);
        launchAiInference(engine, taskId, inputPath, outputPath, message);
    }
}
```

---

## 问题二：重新处理失败任务时崩溃，状态异常

### 症状描述
对全部处理失败的任务执行重新处理操作时，系统发生崩溃闪退，同时出现数据状态异常：未实际处理的任务错误地显示为"处理中"状态。

### 根本原因分析

#### 1. 状态重置不完整
**位置**: `ProcessingController.cpp`

```cpp
void ProcessingController::retryFailedFiles(const QString& messageId) {
    // ...
    m_messageModel->updateStatus(messageId, static_cast<int>(ProcessingStatus::Processing));
    
    for (const auto& file : filesToRetry) {
        m_messageModel->updateFileStatus(messageId, file.id,
            static_cast<int>(ProcessingStatus::Pending), QString());
        // 问题：没有重置 file.resultPath
        // 问题：没有清除 file.errorMessage
    }
}
```

**问题**:
- 文件状态重置不完整，残留旧数据
- 消息状态更新与任务添加不是原子操作
- 没有验证任务是否成功入队

#### 2. 任务上下文残留
**位置**: `ProcessingController.cpp`

```cpp
void ProcessingController::cleanupTask(const QString& taskId) {
    // 清理顺序问题
    AIEngine* engine = m_aiEnginePool->engineForTask(taskId);
    if (engine) {
        engine->forceCancel();
        engine->cleanupGpuMemory();
        engine->resetState();
    }
    
    disconnectAiEngineForTask(taskId);
    m_aiEnginePool->release(taskId);
    // ...
    m_taskMessages.remove(taskId);  // 最后才移除
}
```

**问题**:
- 清理过程中其他代码可能仍在访问 `m_taskMessages`
- 没有等待正在进行的操作完成

#### 3. 状态同步竞态条件
**位置**: `ProcessingController.cpp`

```cpp
void ProcessingController::syncMessageStatus(const QString& messageId, const QString& sessionId) {
    // 防抖逻辑
    const qint64 lastSyncMs = m_lastMessageStatusSyncMs.value(messageId, 0);
    if (lastSyncMs > 0 && (nowMs - lastSyncMs) < kMessageStatusSyncDebounceMs) {
        return;  // 可能跳过重要的状态更新
    }
    // ...
}
```

**问题**:
- 防抖机制可能跳过关键状态更新
- 状态同步是异步的，可能导致显示不一致

### 解决方案

#### 方案 2.1: 完整的状态重置机制

**修改文件**: `ProcessingController.cpp`

```cpp
void ProcessingController::retryFailedFiles(const QString& messageId) {
    if (!m_messageModel) {
        qWarning() << "[ProcessingController] retryFailedFiles: MessageModel not set";
        return;
    }

    Message message = m_messageModel->messageById(messageId);
    if (message.id.isEmpty()) {
        qWarning() << "[ProcessingController] retryFailedFiles: Message not found:" << messageId;
        return;
    }

    // 【修复】完整重置消息状态
    message.status = ProcessingStatus::Pending;
    message.progress = 0;
    message.errorMessage.clear();
    
    QList<MediaFile> filesToRetry;
    for (auto& file : message.mediaFiles) {
        if (file.status == ProcessingStatus::Failed || 
            file.status == ProcessingStatus::Cancelled) {
            // 【修复】完整重置文件状态
            file.status = ProcessingStatus::Pending;
            file.resultPath.clear();
            file.errorMessage.clear();  // 假设 MediaFile 有此字段
            filesToRetry.append(file);
        }
    }

    if (filesToRetry.isEmpty()) {
        return;
    }

    // 【修复】原子更新消息到模型
    m_messageModel->updateMessage(messageId, message);
    
    const QString sessionId = resolveSessionIdForMessage(messageId);
    
    // 【修复】使用事务式任务添加
    QList<QString> addedTaskIds;
    bool allAdded = true;
    
    for (const auto& file : filesToRetry) {
        QueueTask task;
        task.taskId = generateTaskId();
        task.messageId = messageId;
        task.fileId = file.id;
        task.position = m_tasks.size();
        task.progress = 0;
        task.queuedAt = QDateTime::currentDateTime();
        task.status = ProcessingStatus::Pending;

        m_tasks.append(task);
        m_processingModel->addTask(task);
        m_taskToMessage[task.taskId] = messageId;
        
        // 【修复】使用更新后的 message 副本
        m_taskMessages[task.taskId] = message;

        qint64 estimatedMemory = estimateMemoryUsage(file.filePath, file.type);
        registerTaskContext(task.taskId, messageId, sessionId, file.id, estimatedMemory);

        addedTaskIds.append(task.taskId);
        emit taskAdded(task.taskId);
    }
    
    // 【修复】如果添加失败，回滚所有更改
    if (!allAdded) {
        for (const QString& tid : addedTaskIds) {
            cleanupTask(tid);
            m_tasks.removeIf([&](const QueueTask& t) { return t.taskId == tid; });
        }
        m_messageModel->updateStatus(messageId, static_cast<int>(ProcessingStatus::Failed));
        return;
    }

    updateQueuePositions();
    emit queueSizeChanged();

    if (m_sessionController) {
        requestSessionSync();
    }

    processNextTask();
}
```

#### 方案 2.2: 任务状态管理器增强

**修改文件**: `TaskStateManager.cpp`

```cpp
// 新增方法：验证任务状态一致性
bool TaskStateManager::validateTaskConsistency(const QString& messageId, 
                                                 const QString& expectedStatus) const {
    QMutexLocker locker(&m_mutex);
    
    if (!m_messageToTasks.contains(messageId)) {
        return true;  // 没有活动任务，视为一致
    }
    
    const QSet<QString>& taskIds = m_messageToTasks[messageId];
    for (const QString& taskId : taskIds) {
        if (m_activeTasks.contains(taskId)) {
            const ActiveTaskInfo& info = m_activeTasks[taskId];
            // 检查任务是否超时
            qint64 nowMs = QDateTime::currentMSecsSinceEpoch();
            if (nowMs - info.lastUpdateMs > 60000) {  // 1分钟无更新
                qWarning() << "[TaskStateManager] Stale task detected:" << taskId;
                return false;
            }
        }
    }
    
    return true;
}

// 新增方法：清理僵尸任务
void TaskStateManager::cleanupStaleTasks(qint64 timeoutMs) {
    QMutexLocker locker(&m_mutex);
    
    QList<QString> staleTaskIds;
    qint64 nowMs = QDateTime::currentMSecsSinceEpoch();
    
    for (auto it = m_activeTasks.begin(); it != m_activeTasks.end(); ++it) {
        if (nowMs - it->lastUpdateMs > timeoutMs) {
            staleTaskIds.append(it.key());
        }
    }
    
    for (const QString& taskId : staleTaskIds) {
        qWarning() << "[TaskStateManager] Cleaning up stale task:" << taskId;
        unregisterActiveTask(taskId);
    }
}
```

#### 方案 2.3: 状态同步机制重构

**修改文件**: `ProcessingController.cpp`

```cpp
// 新增：状态同步队列
struct StateSyncRequest {
    QString messageId;
    QString sessionId;
    ProcessingStatus status;
    int progress;
    qint64 timestamp;
};

QQueue<StateSyncRequest> m_stateSyncQueue;
QTimer* m_stateSyncTimer;

void ProcessingController::queueStateSync(const QString& messageId, 
                                           const QString& sessionId,
                                           ProcessingStatus status,
                                           int progress) {
    StateSyncRequest req;
    req.messageId = messageId;
    req.sessionId = sessionId;
    req.status = status;
    req.progress = progress;
    req.timestamp = QDateTime::currentMSecsSinceEpoch();
    
    m_stateSyncQueue.enqueue(req);
    
    if (!m_stateSyncTimer->isActive()) {
        m_stateSyncTimer->start(50);  // 50ms 后批量处理
    }
}

void ProcessingController::processStateSyncQueue() {
    QHash<QString, StateSyncRequest> latestRequests;
    
    // 合并同一消息的最新状态
    while (!m_stateSyncQueue.isEmpty()) {
        StateSyncRequest req = m_stateSyncQueue.dequeue();
        if (!latestRequests.contains(req.messageId) ||
            req.timestamp > latestRequests[req.messageId].timestamp) {
            latestRequests[req.messageId] = req;
        }
    }
    
    // 批量更新
    for (const auto& req : latestRequests) {
        updateStatusForSessionMessage(req.sessionId, req.messageId, req.status);
        updateProgressForSessionMessage(req.sessionId, req.messageId, req.progress);
    }
}
```

---

## 问题三：循环压力测试崩溃

### 症状描述
在循环压力测试场景下（删除消息卡片中正在处理的任务，立即处理下一个消息卡片中的任务），处理多媒体文件最终仍会导致系统崩溃闪退。

### 根本原因分析

#### 1. 删除消息时取消不彻底
**位置**: `ProcessingController.cpp`

```cpp
void ProcessingController::cancelMessageTasks(const QString& messageId) {
    // ...
    if (oldStatus == ProcessingStatus::Processing) {
        m_taskCoordinator->requestCancellation(taskId);
        
        if (m_taskMessages.contains(taskId)) {
            const Message& msg = m_taskMessages[taskId];
            if (msg.mode == ProcessingMode::AIInference) {
                if (m_activeAIVideoProcessors.contains(taskId)) {
                    auto processor = m_activeAIVideoProcessors.value(taskId);
                    if (processor) {
                        processor->cancel();  // 异步取消
                    }
                }
                AIEngine* poolEngine = m_aiEnginePool->engineForTask(taskId);
                if (poolEngine) {
                    poolEngine->forceCancel();
                    poolEngine->cleanupGpuMemory();  // 可能在推理线程中调用
                }
            }
        }
        
        m_tasks[i].status = ProcessingStatus::Cancelled;
        cleanupTask(taskId);
        emit taskCancelled(taskId);
        m_tasks.removeAt(i);
        cancelledProcessingCount++;
    }
}
```

**问题**:
- `processor->cancel()` 是异步的，没有等待完成
- `cleanupGpuMemory()` 可能在推理线程中调用，导致竞争
- 没有等待引擎真正停止

#### 2. 引擎清理顺序问题
**位置**: `ProcessingController.cpp`

```cpp
void ProcessingController::cleanupTask(const QString& taskId) {
    // ...
    AIEngine* engine = m_aiEnginePool->engineForTask(taskId);
    if (engine) {
        engine->forceCancel();
        engine->cleanupGpuMemory();
        engine->resetState();
    }
    
    disconnectAiEngineForTask(taskId);
    m_aiEnginePool->release(taskId);
    // ...
}
```

**问题**:
- `forceCancel()` 只是设置标志，不等待推理停止
- `cleanupGpuMemory()` 可能在推理线程仍在使用 GPU 资源时调用
- `resetState()` 可能在推理线程仍在访问状态时调用

#### 3. 快速任务切换时的竞态条件
**位置**: `AIEnginePool.cpp`

```cpp
void AIEnginePool::release(const QString& taskId) {
    QMutexLocker locker(&m_mutex);
    
    if (!m_taskToSlot.contains(taskId)) {
        return;
    }
    
    int idx = m_taskToSlot.take(taskId);
    if (idx >= 0 && idx < m_slots.size()) {
        if (m_slots[idx].engine) {
            m_slots[idx].engine->resetState();
            m_slots[idx].engine->cleanupGpuMemory();
        }
        
        m_slots[idx].inUse = false;
        m_slots[idx].taskId.clear();
        // ...
    }
}
```

**问题**:
- 释放和重新获取引擎之间没有同步
- 清理操作可能与新任务的初始化重叠

### 解决方案

#### 方案 3.1: 安全的任务取消流程

**修改文件**: `ProcessingController.cpp`

```cpp
void ProcessingController::cancelMessageTasks(const QString& messageId) {
    m_taskCoordinator->cancelMessageTasks(messageId);
    
    QList<QString> tasksToWait;
    int cancelledProcessingCount = 0;
    
    // 第一阶段：标记所有任务为取消状态，发送取消请求
    for (int i = m_tasks.size() - 1; i >= 0; --i) {
        if (m_tasks[i].messageId != messageId) {
            continue;
        }
        
        const QString taskId = m_tasks[i].taskId;
        const ProcessingStatus oldStatus = m_tasks[i].status;
        
        if (oldStatus == ProcessingStatus::Pending) {
            m_tasks[i].status = ProcessingStatus::Cancelled;
            cleanupTask(taskId);
            emit taskCancelled(taskId);
            m_tasks.removeAt(i);
        } else if (oldStatus == ProcessingStatus::Processing) {
            // 【修复】先标记为取消，不立即清理
            m_tasks[i].status = ProcessingStatus::Cancelled;
            tasksToWait.append(taskId);
            
            // 发送取消请求
            if (m_taskMessages.contains(taskId)) {
                const Message& msg = m_taskMessages[taskId];
                if (msg.mode == ProcessingMode::AIInference) {
                    // 取消视频处理器
                    if (m_activeAIVideoProcessors.contains(taskId)) {
                        auto processor = m_activeAIVideoProcessors.value(taskId);
                        if (processor) {
                            processor->cancel();
                        }
                    }
                    // 取消 AI 引擎
                    AIEngine* poolEngine = m_aiEnginePool->engineForTask(taskId);
                    if (poolEngine) {
                        poolEngine->forceCancel();
                    }
                }
            }
        }
    }
    
    // 第二阶段：等待所有处理中的任务完成取消
    QElapsedTimer waitTimer;
    waitTimer.start();
    const int kMaxWaitMs = 3000;  // 最多等待3秒
    
    while (!tasksToWait.isEmpty() && waitTimer.elapsed() < kMaxWaitMs) {
        QMutableListIterator<QString> it(tasksToWait);
        while (it.hasNext()) {
            QString taskId = it.next();
            
            // 检查任务是否已完成
            bool stillProcessing = false;
            
            if (m_activeAIVideoProcessors.contains(taskId)) {
                auto processor = m_activeAIVideoProcessors.value(taskId);
                if (processor && processor->isProcessing()) {
                    stillProcessing = true;
                }
            }
            
            if (m_aiEnginePool->engineForTask(taskId)) {
                AIEngine* engine = m_aiEnginePool->engineForTask(taskId);
                if (engine && engine->isProcessing()) {
                    stillProcessing = true;
                }
            }
            
            if (!stillProcessing) {
                // 任务已停止，可以安全清理
                cleanupTask(taskId);
                emit taskCancelled(taskId);
                
                // 从任务列表中移除
                m_tasks.removeIf([&](const QueueTask& t) { return t.taskId == taskId; });
                cancelledProcessingCount++;
                it.remove();
            }
        }
        
        if (!tasksToWait.isEmpty()) {
            QThread::msleep(50);
        }
    }
    
    // 第三阶段：强制清理超时的任务
    for (const QString& taskId : tasksToWait) {
        qWarning() << "[ProcessingController] Force cleaning up task:" << taskId;
        cleanupTask(taskId);
        emit taskCancelled(taskId);
        m_tasks.removeIf([&](const QueueTask& t) { return t.taskId == taskId; });
        cancelledProcessingCount++;
    }
    
    // 修正处理计数器
    if (cancelledProcessingCount > 0 && m_currentProcessingCount > 0) {
        m_currentProcessingCount = qMax(0, m_currentProcessingCount - cancelledProcessingCount);
        emit currentProcessingCountChanged();
    }
    
    updateQueuePositions();
    syncModelTasks();
    emit queueSizeChanged();
    
    if (m_messageModel) {
        m_messageModel->updateStatus(messageId, static_cast<int>(ProcessingStatus::Cancelled));
    }
    
    emit messageTasksCancelled(messageId);
    processNextTask();
}
```

#### 方案 3.2: 引擎安全清理机制

**修改文件**: `AIEngine.cpp`

```cpp
// 新增：安全清理方法
void AIEngine::safeCleanup() {
    qInfo() << "[AIEngine][DEBUG] safeCleanup() called";
    
    // 第一步：等待推理完成
    QElapsedTimer waitTimer;
    waitTimer.start();
    const int kMaxWaitMs = 2000;
    
    while (m_isProcessing.load() && waitTimer.elapsed() < kMaxWaitMs) {
        QThread::msleep(10);
    }
    
    if (m_isProcessing.load()) {
        qWarning() << "[AIEngine] safeCleanup: inference still running after timeout";
    }
    
    // 第二步：获取推理互斥锁，确保没有正在进行的推理
    {
        QMutexLocker inferenceLocker(&m_inferenceMutex);
        // 持有锁后立即释放，确保推理已完成
    }
    
    // 第三步：清理 GPU 内存（在主线程安全执行）
#if NCNN_VULKAN
    if (m_vkdev) {
        QMutexLocker locker(&m_mutex);
        
        if (m_blobVkAllocator) {
            m_blobVkAllocator->clear();
        }
        if (m_stagingVkAllocator) {
            m_stagingVkAllocator->clear();
        }
    }
#endif
    
    // 第四步：重置状态
    m_cancelRequested.store(false);
    m_forceCancelled.store(false);
    m_gpuOomDetected.store(false);
    m_isProcessing.store(false);
    m_progress.store(0.0);
    
    clearParameters();
    
    if (m_progressReporter) {
        m_progressReporter->reset();
    }
    
    qInfo() << "[AIEngine][DEBUG] safeCleanup() completed";
}
```

**修改文件**: `AIEnginePool.cpp`

```cpp
void AIEnginePool::release(const QString& taskId) {
    QMutexLocker locker(&m_mutex);
    
    if (!m_taskToSlot.contains(taskId)) {
        return;
    }
    
    int idx = m_taskToSlot.take(taskId);
    if (idx >= 0 && idx < m_slots.size()) {
        if (m_slots[idx].engine) {
            // 【修复】使用安全清理方法
            m_slots[idx].engine->safeCleanup();
        }
        
        m_slots[idx].inUse = false;
        m_slots[idx].taskId.clear();
        m_slots[idx].state = EngineState::Ready;

        qInfo() << "[AIEnginePool][DEBUG] Task released:" << taskId 
                << ", slot index:" << idx;
        emit engineReleased(taskId, idx);
    }
}
```

#### 方案 3.3: 任务切换同步机制

**修改文件**: `ProcessingController.cpp`

```cpp
// 新增：任务切换同步器
class TaskSwitchSynchronizer : public QObject {
    Q_OBJECT
    
public:
    static TaskSwitchSynchronizer* instance() {
        static TaskSwitchSynchronizer* s_instance = nullptr;
        if (!s_instance) {
            s_instance = new TaskSwitchSynchronizer();
        }
        return s_instance;
    }
    
    void beginTaskSwitch(const QString& oldTaskId, const QString& newTaskId) {
        QMutexLocker locker(&m_mutex);
        
        m_currentSwitch = {oldTaskId, newTaskId, QDateTime::currentMSecsSinceEpoch()};
        
        qInfo() << "[TaskSwitchSynchronizer] Begin switch from" << oldTaskId 
                << "to" << newTaskId;
    }
    
    void endTaskSwitch(const QString& taskId) {
        QMutexLocker locker(&m_mutex);
        
        if (m_currentSwitch.newTaskId == taskId || m_currentSwitch.oldTaskId == taskId) {
            m_currentSwitch = {};
            m_switchCompleteCv.notify_all();
        }
    }
    
    bool waitForSwitchComplete(int timeoutMs = 5000) {
        std::unique_lock<std::mutex> lock(m_switchMutex);
        return m_switchCompleteCv.wait_for(lock, std::chrono::milliseconds(timeoutMs),
            [this]() { return m_currentSwitch.oldTaskId.isEmpty(); });
    }
    
private:
    TaskSwitchSynchronizer() = default;
    
    struct SwitchInfo {
        QString oldTaskId;
        QString newTaskId;
        qint64 timestamp;
    };
    
    mutable QMutex m_mutex;
    std::mutex m_switchMutex;
    std::condition_variable m_switchCompleteCv;
    SwitchInfo m_currentSwitch;
};

// 在 processNextTask 中使用
void ProcessingController::processNextTask() {
    if (m_queueStatus != QueueStatus::Running) {
        return;
    }

    validateAndRepairQueueState();

    if (m_currentProcessingCount >= 1) {
        return;
    }

    QueueTask* nextTask = nullptr;
    for (auto& task : m_tasks) {
        if (task.status != ProcessingStatus::Pending) {
            continue;
        }
        
        if (m_taskMessages.contains(task.taskId)) {
            const Message& msg = m_taskMessages[task.taskId];
            if (msg.mode == ProcessingMode::AIInference && m_aiEnginePool->availableCount() <= 0) {
                continue;
            }
        }
        
        nextTask = &task;
        break;
    }

    if (!nextTask) {
        return;
    }

    // 【新增】等待之前的任务切换完成
    if (!TaskSwitchSynchronizer::instance()->waitForSwitchComplete(3000)) {
        qWarning() << "[ProcessingController] Task switch timeout, proceeding anyway";
    }

    if (!tryStartTask(*nextTask)) {
        QTimer::singleShot(100, this, &ProcessingController::processNextTask);
    }
}
```

---

## 设计模式重构建议

### 1. 状态机模式 (State Pattern)

**目的**: 将任务状态管理从分散的条件判断重构为状态机模式

```cpp
// 任务状态基类
class TaskState {
public:
    virtual ~TaskState() = default;
    virtual void enter(ProcessingController* controller, const QString& taskId) = 0;
    virtual void exit(ProcessingController* controller, const QString& taskId) = 0;
    virtual bool canTransitionTo(TaskState* newState) const = 0;
    virtual ProcessingStatus status() const = 0;
};

// 具体状态类
class PendingState : public TaskState { /* ... */ };
class ProcessingState : public TaskState { /* ... */ };
class CompletedState : public TaskState { /* ... */ };
class FailedState : public TaskState { /* ... */ };
class CancelledState : public TaskState { /* ... */ };

// 任务上下文
class TaskContext {
private:
    std::unique_ptr<TaskState> m_state;
    
public:
    void transitionTo(std::unique_ptr<TaskState> newState);
    TaskState* state() const { return m_state.get(); }
};
```

### 2. 命令模式 (Command Pattern)

**目的**: 将任务操作封装为命令对象，支持撤销和重试

```cpp
// 任务命令接口
class TaskCommand {
public:
    virtual ~TaskCommand() = default;
    virtual bool execute() = 0;
    virtual bool undo() = 0;
    virtual bool redo() = 0;
    virtual QString description() const = 0;
};

// AI推理命令
class AIInferenceCommand : public TaskCommand {
private:
    QString m_inputPath;
    QString m_outputPath;
    QString m_modelId;
    AIEngine* m_engine;
    
public:
    bool execute() override;
    bool undo() override;
    bool redo() override;
};

// 命令队列
class TaskCommandQueue {
private:
    QQueue<std::shared_ptr<TaskCommand>> m_commands;
    QStack<std::shared_ptr<TaskCommand>> m_undoStack;
    
public:
    void enqueue(std::shared_ptr<TaskCommand> command);
    bool executeNext();
    bool undoLast();
};
```

### 3. 观察者模式增强 (Observer Pattern)

**目的**: 解耦状态通知和 UI 更新

```cpp
// 任务事件类型
enum class TaskEventType {
    Started,
    ProgressUpdated,
    Completed,
    Failed,
    Cancelled
};

// 任务事件
struct TaskEvent {
    QString taskId;
    TaskEventType type;
    QVariant data;
    qint64 timestamp;
};

// 事件监听器接口
class TaskEventListener {
public:
    virtual ~TaskEventListener() = default;
    virtual void onTaskEvent(const TaskEvent& event) = 0;
};

// 事件分发器
class TaskEventDispatcher {
private:
    QList<TaskEventListener*> m_listeners;
    QQueue<TaskEvent> m_eventQueue;
    QTimer* m_dispatchTimer;
    
public:
    void addListener(TaskEventListener* listener);
    void removeListener(TaskEventListener* listener);
    void dispatchEvent(const TaskEvent& event);
};
```

### 4. 熔断器模式 (Circuit Breaker)

**目的**: 防止连续失败导致系统崩溃

```cpp
// 熔断器状态
enum class CircuitState {
    Closed,     // 正常
    Open,       // 熔断
    HalfOpen    // 半开（尝试恢复）
};

// 熔断器
class TaskCircuitBreaker {
private:
    CircuitState m_state = CircuitState::Closed;
    int m_failureCount = 0;
    int m_successCount = 0;
    qint64 m_lastFailureTime = 0;
    
    const int m_failureThreshold = 5;
    const int m_successThreshold = 3;
    const qint64 m_resetTimeoutMs = 30000;
    
public:
    bool allowExecution() {
        switch (m_state) {
        case CircuitState::Closed:
            return true;
        case CircuitState::Open:
            if (QDateTime::currentMSecsSinceEpoch() - m_lastFailureTime > m_resetTimeoutMs) {
                m_state = CircuitState::HalfOpen;
                return true;
            }
            return false;
        case CircuitState::HalfOpen:
            return true;
        }
        return false;
    }
    
    void recordSuccess() {
        m_failureCount = 0;
        if (m_state == CircuitState::HalfOpen) {
            m_successCount++;
            if (m_successCount >= m_successThreshold) {
                m_state = CircuitState::Closed;
                m_successCount = 0;
            }
        }
    }
    
    void recordFailure() {
        m_failureCount++;
        m_lastFailureTime = QDateTime::currentMSecsSinceEpoch();
        
        if (m_state == CircuitState::HalfOpen) {
            m_state = CircuitState::Open;
        } else if (m_failureCount >= m_failureThreshold) {
            m_state = CircuitState::Open;
        }
    }
};
```

---

## 实施计划

### 阶段一：紧急修复（优先级：高）

1. **Vulkan 初始化同步机制**
   - 修改文件: `AIEngine.cpp`, `AIEngine.h`
   - 预计工作量: 4小时
   - 测试要点: 首次推理稳定性

2. **引擎安全清理机制**
   - 修改文件: `AIEngine.cpp`, `AIEnginePool.cpp`
   - 预计工作量: 3小时
   - 测试要点: 任务取消后资源释放

3. **任务取消流程重构**
   - 修改文件: `ProcessingController.cpp`
   - 预计工作量: 4小时
   - 测试要点: 循环压力测试稳定性

### 阶段二：状态管理优化（优先级：中）

1. **状态同步机制重构**
   - 修改文件: `ProcessingController.cpp`, `ProcessingController.h`
   - 预计工作量: 6小时
   - 测试要点: 状态显示一致性

2. **任务状态管理器增强**
   - 修改文件: `TaskStateManager.cpp`, `TaskStateManager.h`
   - 预计工作量: 4小时
   - 测试要点: 僵尸任务检测和清理

### 阶段三：架构重构（优先级：低）

1. **状态机模式重构**
   - 新增文件: `TaskState.h`, `TaskState.cpp`
   - 预计工作量: 8小时
   - 测试要点: 所有状态转换正确性

2. **熔断器模式实现**
   - 新增文件: `TaskCircuitBreaker.h`, `TaskCircuitBreaker.cpp`
   - 预计工作量: 4小时
   - 测试要点: 连续失败场景处理

---

## 预期成果

1. **首次处理稳定性**: 99%+ 成功率
2. **任务重试稳定性**: 100% 状态一致性
3. **压力测试稳定性**: 连续 100 次循环无崩溃
4. **内存泄漏**: 无新增内存泄漏
5. **响应时间**: 任务切换延迟 < 100ms

---

## 风险评估

| 风险项 | 影响 | 概率 | 缓解措施 |
|--------|------|------|----------|
| Vulkan 驱动兼容性 | 高 | 中 | 增加更多驱动检测和降级逻辑 |
| 线程死锁 | 高 | 低 | 使用死锁检测工具，仔细设计锁顺序 |
| 性能回退 | 中 | 低 | 性能基准测试，关键路径优化 |
| 状态机复杂度增加 | 中 | 中 | 充分单元测试，状态图文档化 |

---

## 文档更新

完成实施后需要更新以下文档：

1. `docs/Notes/AI_Inference_Architecture.md` - AI 推理架构文档
2. `docs/Notes/Task_Management_Design.md` - 任务管理设计文档
3. `.trae/rules/07-performance-threading.md` - 性能与线程规则
4. `README.md` - 已知问题列表更新
