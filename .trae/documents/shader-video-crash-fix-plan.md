# Shader模式视频处理崩溃问题修复计划

## 问题分析

### 症状
- Shader模式消息卡片中点击"重新处理"按钮时，应用程序崩溃
- 上传视频类型多媒体文件使用Shader模式处理时，应用程序崩溃

### 根本原因

经过代码分析，发现以下关键问题：

#### 1. VideoProcessor 对象生命周期管理缺陷

**位置**: `ProcessingController::processShaderVideoThumbnailAsync` (ProcessingController.cpp:805-899)

**问题**:
```cpp
// 在 m_threadPool 的 lambda 中创建 VideoProcessor
auto* videoProcessor = new VideoProcessor();

// finishCb 回调中调用 deleteLater
videoProcessor->deleteLater();
```

`VideoProcessor` 对象在 `m_threadPool` 线程中创建，但 `deleteLater()` 在 `QtConcurrent::run` 的另一个线程中调用。这导致：
- 对象可能在线程仍在使用时被销毁
- Qt 事件循环无法正确处理跨线程的对象销毁

#### 2. 线程池嵌套使用导致死锁风险

**位置**: `ProcessingController::processShaderVideoThumbnailAsync`

**问题**:
- 外层: `m_threadPool->start([...])` - 线程池线程
- 内层: `QtConcurrent::run([...])` - 全局线程池

这种嵌套结构可能导致：
- 线程池资源耗尽
- 死锁或竞态条件
- 内存访问冲突

#### 3. 回调函数捕获悬空指针

**位置**: `ProcessingController.cpp:861-895`

**问题**:
```cpp
auto finishCb = [this, videoProcessor, taskId, ...](bool success, ...) {
    QMetaObject::invokeMethod(this, [this, videoProcessor, ...]() {
        videoProcessor->deleteLater();  // videoProcessor 可能已失效
        ...
    }, Qt::QueuedConnection);
};
```

当回调执行时，`videoProcessor` 指针可能已经失效，导致崩溃。

#### 4. 重新处理时任务上下文不完整

**位置**: `ProcessingController::retrySingleFailedFile` (ProcessingController.cpp:1132-1203)

**问题**:
- 重新处理时，`m_taskMessages[task.taskId] = message` 存储了消息的副本
- 但消息中的 `mediaFiles` 列表可能已经过时
- 导致处理时使用错误的文件路径或参数

## 修复方案

### 方案一：重构 VideoProcessor 生命周期管理（推荐）

#### 修改 1: 使用 QSharedPointer 管理 VideoProcessor

**文件**: `ProcessingController.h`

```cpp
// 在 private 成员中添加
QHash<QString, QSharedPointer<VideoProcessor>> m_activeVideoProcessors;
```

**文件**: `ProcessingController.cpp`

```cpp
void ProcessingController::processShaderVideoThumbnailAsync(...) {
    // 创建共享指针管理的 VideoProcessor
    auto videoProcessor = QSharedPointer<VideoProcessor>::create();
    m_activeVideoProcessors[taskId] = videoProcessor;
    
    // 使用 QSharedPointer 捕获，确保生命周期安全
    auto progressCb = [this, taskId, videoProcessor](int progress, const QString&) {
        // 检查 videoProcessor 是否仍然有效
        if (!m_activeVideoProcessors.contains(taskId)) return;
        ...
    };
    
    auto finishCb = [this, videoProcessor, taskId, ...](bool success, ...) {
        QMetaObject::invokeMethod(this, [this, videoProcessor, taskId, ...]() {
            // 从活动列表中移除，QSharedPointer 自动管理销毁
            m_activeVideoProcessors.remove(taskId);
            ...
        }, Qt::QueuedConnection);
    };
    
    videoProcessor->processVideoAsync(filePath, outputPath, shaderParams, progressCb, finishCb);
}
```

#### 修改 2: 移除嵌套线程池调用

将 `m_threadPool->start` + `QtConcurrent::run` 改为直接使用 `QtConcurrent::run`：

```cpp
void ProcessingController::processShaderVideoThumbnailAsync(...) {
    auto videoProcessor = QSharedPointer<VideoProcessor>::create();
    m_activeVideoProcessors[taskId] = videoProcessor;
    
    // 直接使用 QtConcurrent::run，避免嵌套
    QtConcurrent::run([this, taskId, videoProcessor, filePath, outputPath, shaderParams, ...]() {
        // 先生成缩略图
        QImage videoThumb = ImageUtils::generateVideoThumbnail(filePath, QSize(512, 512));
        ...
        
        // 设置缩略图
        QMetaObject::invokeMethod(this, [this, fileId, processedThumb]() {
            if (ThumbnailProvider::instance() && !processedThumb.isNull()) {
                const QString thumbId = "processed_" + fileId;
                ThumbnailProvider::instance()->setThumbnail(thumbId, processedThumb);
            }
        }, Qt::QueuedConnection);
        
        // 处理视频
        auto progressCb = [this, taskId, videoProcessor](int progress, const QString&) {
            if (!m_activeVideoProcessors.contains(taskId)) return;
            int mappedProgress = 10 + progress * 85 / 100;
            QMetaObject::invokeMethod(this, [this, taskId, mappedProgress]() {
                updateTaskProgress(taskId, mappedProgress);
            }, Qt::QueuedConnection);
        };
        
        auto finishCb = [this, videoProcessor, taskId, sessionId, messageId, fileId, filePath, outputPath](
                            bool success, const QString& resultPath, const QString& error) {
            QMetaObject::invokeMethod(this, [this, videoProcessor, taskId, sessionId, messageId,
                                              fileId, filePath, outputPath, success, resultPath, error]() {
                m_activeVideoProcessors.remove(taskId);
                ...
            }, Qt::QueuedConnection);
        };
        
        videoProcessor->processVideoAsync(filePath, outputPath, shaderParams, progressCb, finishCb);
    });
}
```

#### 修改 3: 添加取消处理支持

**文件**: `ProcessingController.h`

```cpp
void cancelVideoProcessing(const QString& taskId);
```

**文件**: `ProcessingController.cpp`

```cpp
void ProcessingController::cancelVideoProcessing(const QString& taskId) {
    if (m_activeVideoProcessors.contains(taskId)) {
        auto processor = m_activeVideoProcessors[taskId];
        if (processor) {
            processor->cancel();
        }
        m_activeVideoProcessors.remove(taskId);
    }
}
```

#### 修改 4: 在 cleanupTask 中清理 VideoProcessor

```cpp
void ProcessingController::cleanupTask(const QString& taskId) {
    // 取消并移除活动的视频处理器
    cancelVideoProcessing(taskId);
    
    // 原有清理逻辑
    m_resourceManager->release(taskId);
    ...
}
```

### 方案二：修复重新处理逻辑

#### 修改: 在 retrySingleFailedFile 中刷新消息数据

**文件**: `ProcessingController.cpp`

```cpp
void ProcessingController::retrySingleFailedFile(const QString& messageId, int fileIndex) {
    ...
    
    // 从 Session 获取最新的消息数据
    const QString sessionId = resolveSessionIdForMessage(messageId);
    Message message = messageForSession(sessionId, messageId);
    
    if (message.id.isEmpty()) {
        qWarning() << "[ProcessingController] retrySingleFailedFile: Message not found:" << messageId;
        return;
    }
    
    // 验证文件索引有效性
    if (fileIndex < 0 || fileIndex >= message.mediaFiles.size()) {
        qWarning() << "[ProcessingController] retrySingleFailedFile: Invalid file index:" << fileIndex;
        return;
    }
    
    const MediaFile& file = message.mediaFiles[fileIndex];
    ...
}
```

### 方案三：增强错误处理和日志

#### 修改: 添加详细的调试日志

在关键位置添加日志输出，帮助定位问题：

```cpp
void ProcessingController::processShaderVideoThumbnailAsync(...) {
    qInfo() << "[ProcessingController][Shader] Starting video processing"
            << "task:" << taskId
            << "input:" << filePath
            << "output:" << outputPath;
    
    auto finishCb = [this, videoProcessor, taskId, ...](bool success, ...) {
        qInfo() << "[ProcessingController][Shader] Video processing callback"
                << "task:" << taskId
                << "success:" << success
                << "processor valid:" << !!videoProcessor.data();
        ...
    };
}
```

## 实施步骤

### 第一阶段：核心修复
1. 在 `ProcessingController.h` 中添加 `m_activeVideoProcessors` 成员
2. 重构 `processShaderVideoThumbnailAsync` 使用 QSharedPointer
3. 移除嵌套线程池调用
4. 添加 `cancelVideoProcessing` 方法
5. 在 `cleanupTask` 中调用 `cancelVideoProcessing`

### 第二阶段：重新处理修复
1. 修改 `retrySingleFailedFile` 使用最新的消息数据
2. 添加文件路径有效性验证
3. 确保任务上下文正确初始化

### 第三阶段：测试验证
1. 构建项目
2. 测试 Shader 模式视频处理
3. 测试重新处理失败文件
4. 检查日志输出确认无错误
5. 验证内存无泄漏

## 风险评估

### 低风险
- QSharedPointer 替换原始指针：标准 Qt 内存管理模式
- 添加取消处理：增强健壮性

### 中等风险
- 移除嵌套线程池：可能影响性能，需要测试验证

### 缓解措施
- 分阶段实施，每阶段独立验证
- 保留原有日志输出用于对比
- 添加详细的错误处理和日志

## 预期结果

修复后：
1. Shader 模式视频处理不再崩溃
2. 重新处理失败文件正常工作
3. 内存正确释放，无泄漏
4. 取消操作能正确中断处理
