# 视频处理缩略图和进度条问题修复计划

## 问题概述

根据日志分析发现两个主要问题：

### 问题1: moov atom not found 错误
```
[mov,mp4,m4a,3gp,3g2,mj2 @ 00000257209d6dc0] moov atom not found
[mov,mp4,m4a,3gp,3g2,mj2 @ 000002572EDD5700] moov atom not found
[2026-03-28 05:24:14.649] [WARN] Could not open media. FFmpeg error description: Invalid data found when processing input
```

**原因**: FFmpeg 尝试打开一个尚未完全写入的视频文件。`moov atom` 是 MP4 文件的元数据块，包含视频的索引和时长信息，位于文件末尾。如果文件正在被写入或损坏，FFmpeg 无法找到该 atom。

### 问题2: 缩略图闪烁、跳动，显示占位图，视频无法播放

**根因分析**:
1. **缩略图生成时机错误**: `processShaderVideoThumbnailAsync` 在视频处理开始时就异步生成缩略图，但此时视频文件尚未完成
2. **文件完整性未验证**: 缩略图生成器尝试读取正在写入的视频文件，导致 `moov atom not found` 错误
3. **缩略图缓存未正确更新**: 处理完成后没有重新生成缩略图

### 问题3: 进度条直接显示100%

**根因分析**:
1. `syncMessageProgress` 函数中，对于 Pending 状态的任务没有正确处理
2. 进度计算逻辑：`totalProgress / totalTasks` 在所有任务都是 Pending 时会得到 0，但代码中 Completed/Failed/Cancelled 都加 100，导致计算错误
3. 任务开始时进度未正确初始化

## 修复方案

### 修复1: 调整缩略图生成时机

**文件**: `src/controllers/ProcessingController.cpp`

**修改点**: `processShaderVideoThumbnailAsync` 函数 (第935-1031行)

**修改内容**:
1. 移除视频处理开始时的缩略图生成代码（第957-986行）
2. 在视频处理完成后的 `finishCb` 中生成缩略图
3. 验证输出文件存在且大小 > 0 后再生成缩略图

### 修复2: 增强文件完整性检查

**文件**: `src/providers/ThumbnailProvider.cpp`

**修改点**: `generateThumbnailAsync` 函数 (第178-207行)

**修改内容**:
1. 增加文件大小检查，确保文件不为空
2. 对于视频文件，检查文件是否正在被写入（通过修改时间判断）
3. 如果文件刚被修改（500ms内），延迟或跳过缩略图生成

### 修复3: 修复进度初始化和计算逻辑

**文件**: `src/controllers/ProcessingController.cpp`

**修改点**: 
1. `startTask` 函数 (第606-739行) - 确保任务开始时进度初始化为 0
2. `syncMessageProgress` 函数 (第1709-1750行) - 修复进度计算逻辑

**修改内容**:
1. 在 `startTask` 中设置 `task.progress = 0`
2. 在 `syncMessageProgress` 中：
   - 只统计 Processing 状态任务的进度
   - Pending 状态任务不计入进度计算
   - 确保进度值在 0-100 范围内

## 具体修改步骤

### 步骤1: 修改 ProcessingController.cpp

#### 1.1 修改 processShaderVideoThumbnailAsync 函数

**位置**: 第935-1031行

**修改前**:
```cpp
void ProcessingController::processShaderVideoThumbnailAsync(...)
{
    // ...
    auto videoProcessor = QSharedPointer<VideoProcessor>::create();
    m_activeVideoProcessors[taskId] = videoProcessor;

    // 先异步生成缩略图（不阻塞视频处理）
    QtConcurrent::run([this, fileId, filePath, shaderParams]() {
        QImage videoThumb = ImageUtils::generateVideoThumbnail(filePath, QSize(512, 512));
        // ... 处理缩略图 ...
    });

    // ... 进度和完成回调 ...
}
```

**修改后**:
```cpp
void ProcessingController::processShaderVideoThumbnailAsync(...)
{
    // ...
    auto videoProcessor = QSharedPointer<VideoProcessor>::create();
    m_activeVideoProcessors[taskId] = videoProcessor;

    // 移除开头的缩略图生成代码
    // 缩略图将在视频处理完成后生成

    auto progressCb = [this, taskId](int progress, const QString&) {
        // ... 保持不变 ...
    };

    auto finishCb = [this, videoProcessor, taskId, sessionId, messageId, fileId, outputPath](
                        bool success, const QString& resultPath, const QString& error) {
        QMetaObject::invokeMethod(this, [this, videoProcessor, taskId, sessionId, messageId,
                                          fileId, outputPath, success, error]() {
            // ... 前面的日志和清理代码保持不变 ...

            if (success) {
                // 验证输出文件
                QFileInfo outputInfo(outputPath);
                if (!outputInfo.exists() || outputInfo.size() == 0) {
                    qWarning() << "[ProcessingController][Shader] output file invalid"
                               << "path:" << outputPath
                               << "exists:" << outputInfo.exists()
                               << "size:" << outputInfo.size();
                    success = false;
                    error = tr("视频输出文件无效");
                } else {
                    // 成功后生成缩略图
                    if (ThumbnailProvider::instance()) {
                        const QString thumbId = "processed_" + fileId;
                        ThumbnailProvider::instance()->generateThumbnailAsync(
                            outputPath, thumbId, QSize(512, 512));
                    }
                    
                    updateFileStatusForSessionMessage(sessionId, messageId, fileId,
                        ProcessingStatus::Completed, outputPath);
                }
            } else {
                // ... 失败处理保持不变 ...
            }

            finalizeTask(taskId, sessionId, messageId);
        }, Qt::QueuedConnection);
    };

    videoProcessor->processVideoAsync(filePath, outputPath, shaderParams, progressCb, finishCb);
}
```

#### 1.2 修改 startTask 函数

**位置**: 第606行附近

**修改**: 在任务开始时初始化进度为 0

```cpp
void ProcessingController::startTask(QueueTask& task)
{
    QElapsedTimer perfTimer;
    perfTimer.start();

    task.status = ProcessingStatus::Processing;
    task.startedAt = QDateTime::currentDateTime();
    task.progress = 0;  // 新增：确保进度初始化为 0
    m_currentProcessingCount++;

    // ... 其余代码保持不变 ...
}
```

#### 1.3 修改 syncMessageProgress 函数

**位置**: 第1709-1750行

**修改前**:
```cpp
void ProcessingController::syncMessageProgress(const QString& messageId, const QString& sessionId)
{
    // ... 防抖代码 ...

    int totalTasks = 0;
    int totalProgress = 0;

    for (const auto& task : m_tasks) {
        if (task.messageId == messageId) {
            // ... session 检查 ...

            totalTasks++;
            if (task.status == ProcessingStatus::Completed) {
                totalProgress += 100;
            } else if (task.status == ProcessingStatus::Failed ||
                       task.status == ProcessingStatus::Cancelled) {
                totalProgress += 100;
            } else {
                totalProgress += task.progress;  // Pending 和 Processing 都加 progress
            }
        }
    }

    const int overallProgress = (totalTasks > 0) ? (totalProgress / totalTasks) : 0;
    updateProgressForSessionMessage(targetSessionId, messageId, overallProgress);
}
```

**修改后**:
```cpp
void ProcessingController::syncMessageProgress(const QString& messageId, const QString& sessionId)
{
    // ... 防抖代码保持不变 ...

    int totalTasks = 0;
    int totalProgress = 0;
    int processingTasks = 0;

    for (const auto& task : m_tasks) {
        if (task.messageId == messageId) {
            TaskContext ctx = m_taskCoordinator->getTaskContext(task.taskId);
            if (!ctx.sessionId.isEmpty() && ctx.sessionId != targetSessionId) {
                continue;
            }

            totalTasks++;
            
            if (task.status == ProcessingStatus::Processing) {
                processingTasks++;
                totalProgress += task.progress;
            } else if (task.status == ProcessingStatus::Completed) {
                totalProgress += 100;
            } else if (task.status == ProcessingStatus::Failed ||
                       task.status == ProcessingStatus::Cancelled) {
                totalProgress += 100;
            }
            // Pending 状态的任务不计入进度（不累加）
        }
    }

    int overallProgress = 0;
    if (totalTasks > 0) {
        overallProgress = totalProgress / totalTasks;
    }
    
    // 确保进度在合理范围内
    overallProgress = qBound(0, overallProgress, 100);
    
    updateProgressForSessionMessage(targetSessionId, messageId, overallProgress);
}
```

### 步骤2: 增强 ThumbnailProvider.cpp

**位置**: 第178-207行

**修改内容**:
```cpp
void ThumbnailProvider::generateThumbnailAsync(const QString &filePath, const QString &id, const QSize &size)
{
    {
        QMutexLocker locker(&m_mutex);
        if (m_thumbnails.contains(id) || m_pendingRequests.contains(id)) {
            return;
        }
        
        QFileInfo fileInfo(filePath);
        if (!fileInfo.exists()) {
            qWarning() << "[ThumbnailProvider] File does not exist, skipping thumbnail generation:" << filePath;
            return;
        }
        
        if (fileInfo.size() == 0) {
            qWarning() << "[ThumbnailProvider] File is empty, skipping thumbnail generation:" << filePath;
            return;
        }
        
        // 对于视频文件，检查文件是否正在被写入
        if (ImageUtils::isVideoFile(filePath)) {
            QDateTime lastModified = fileInfo.lastModified();
            qint64 msSinceModified = lastModified.msecsTo(QDateTime::currentDateTime());
            if (msSinceModified < 500) {
                // 文件刚被修改，可能还在写入中，跳过缩略图生成
                qInfo() << "[ThumbnailProvider] Video file recently modified, skipping thumbnail generation:" << filePath;
                return;
            }
        }
        
        m_pendingRequests.insert(id);
        if (id.startsWith("processed_")) {
            m_idToPath[id] = filePath;
        }
    }

    ThumbnailGenerator* generator = new ThumbnailGenerator(filePath, id, size);
    connect(generator, &ThumbnailGenerator::thumbnailReady,
            this, &ThumbnailProvider::onThumbnailReady);
    m_threadPool->start(generator);
}
```

## 验证步骤

修复完成后需要验证：

### 1. 缩略图显示验证
- [ ] 视频处理完成后缩略图正确显示
- [ ] 不再出现闪烁或跳动
- [ ] 不显示占位图
- [ ] 缩略图显示处理后的效果

### 2. 视频播放验证
- [ ] 处理后的视频可以正常播放
- [ ] 视频文件完整性正确
- [ ] 不再出现 `moov atom not found` 错误

### 3. 进度条验证
- [ ] 进度从 0 开始显示
- [ ] 进度根据实际处理情况平滑增长
- [ ] 多文件处理时进度正确计算
- [ ] 处理完成时进度显示 100%

### 4. 日志验证
- [ ] 不再出现 `moov atom not found` 警告
- [ ] 不再出现 `Invalid data found when processing input` 错误
- [ ] 缩略图生成日志正常

## 风险评估

### 低风险
- 进度初始化修改：仅影响显示，不影响功能
- 进度计算逻辑修改：使显示更准确

### 中风险
- 缩略图生成时机修改：需要确保视频处理完成后文件已完全写入
- 文件完整性检查：可能延迟缩略图显示，但更安全

## 回滚方案

如果修复引入新问题：
1. 恢复 `processShaderVideoThumbnailAsync` 中的缩略图生成代码
2. 恢复 `syncMessageProgress` 的原始计算逻辑
3. 移除 `ThumbnailProvider` 中的文件检查代码
