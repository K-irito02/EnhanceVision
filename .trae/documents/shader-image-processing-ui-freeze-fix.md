# Shader 模式处理图片时界面卡顿问题分析与修复计划

## 问题现象

Shader 模式正在处理图片任务时，界面操作会不流畅、有点卡顿。但 Shader 模式处理视频和 AI 推理模式处理图片/视频时不会有这种现象。

## 根本原因分析

### Shader 模式处理图片的当前实现（导致卡顿）

```
ProcessingController::startTask()
    ↓ (Shader 图片模式无特殊处理)
    ↓ QTimer::singleShot(10, ...) → completeTask()
    ↓
ProcessingController::completeTask()
    ↓ emit requestShaderExport(...)
    ↓
ImageExportService::requestExport()
    ↓ emit exportRequested(...)
    ↓
OffscreenShaderRenderer.qml (QML 主线程)
    ↓ sourceImage.source = ... (加载图片)
    ↓ shaderEffect.grabToImage() (GPU 渲染)
    ↓ result.saveToFile() (保存文件)
    ↓
    ❌ 所有操作都在 QML 主线程执行，阻塞 UI！
```

### Shader 模式处理视频的实现（不卡顿）

```
ProcessingController::processShaderVideoThumbnailAsync()
    ↓
VideoProcessor::processVideoAsync()
    ↓ QtConcurrent::run() ← 后台线程
    ↓ ImageProcessor::applyShader() ← CPU 端处理
    ↓ QMetaObject::invokeMethod(Qt::QueuedConnection) ← 异步回调
    ↓
    ✅ 后台线程处理，不阻塞 UI
```

### AI 推理模式的实现（不卡顿）

```
ProcessingController::launchAiInference()
    ↓
AIEngine::processAsync()
    ↓ QtConcurrent::run() ← 后台线程
    ↓ NCNN 推理
    ↓ emit progressChanged/processFileCompleted (Qt::QueuedConnection)
    ↓
    ✅ 后台线程处理，不阻塞 UI
```

## 问题根源

**Shader 模式处理图片时，整个处理流程都在 QML 主线程（GUI 线程）中执行**：

1. 图片加载 (`sourceImage.source = ...`)
2. GPU 渲染 (`grabToImage()`)
3. 文件保存 (`saveToFile()`)

这些操作会阻塞 UI 线程，导致界面卡顿。

而其他模式都使用了后台线程处理，并通过 `Qt::QueuedConnection` 异步更新 UI，所以不会卡顿。

## 解决方案

### 方案选择：后台线程 CPU 处理

将 Shader 图片处理改为与 Shader 视频处理相同的模式：
- 使用 `ImageProcessor::applyShader()` 在后台线程处理
- 通过 `Qt::QueuedConnection` 异步回调主线程

**优点**：
1. 与视频处理模式架构一致
2. 算法与 GPU Shader 完全一致（`ImageProcessor` 已实现）
3. 不阻塞 UI 线程
4. 代码复用度高

### 修改内容

#### 1. ProcessingController.h - 添加成员变量

```cpp
// 添加 ImageProcessor 指针
#include "EnhanceVision/core/ImageProcessor.h"

// 在 private 成员中添加
ImageProcessor* m_imageProcessor;
QHash<QString, QSharedPointer<ImageProcessor>> m_activeImageProcessors;
```

#### 2. ProcessingController.cpp - 构造函数初始化

```cpp
ProcessingController::ProcessingController(QObject* parent)
    : ...
    , m_imageProcessor(new ImageProcessor(this))
{
    // ... 现有代码
}
```

#### 3. ProcessingController.cpp - 修改 startTask() 函数

在 `startTask()` 函数中，为 Shader 模式图片添加后台处理逻辑：

```cpp
void ProcessingController::startTask(QueueTask& task)
{
    // ... 现有代码直到 QString taskId = task.taskId;

    // 检查是否为 Shader 模式图片
    if (m_taskMessages.contains(taskId)) {
        const Message& message = m_taskMessages[taskId];
        
        // Shader 模式图片处理 - 后台线程
        if (message.mode == ProcessingMode::Shader) {
            QString inputPath;
            for (const auto& file : message.mediaFiles) {
                for (const auto& t : m_tasks) {
                    if (t.taskId == taskId && t.fileId == file.id) {
                        inputPath = file.filePath;
                        break;
                    }
                }
                if (!inputPath.isEmpty()) break;
            }
            
            if (inputPath.isEmpty() || !ImageUtils::isImageFile(inputPath)) {
                // 非图片文件，走原有流程
                QTimer::singleShot(10, this, [this, taskId]() {
                    completeTask(taskId, "");
                });
                return;
            }
            
            // 生成输出路径
            QFileInfo fileInfo(inputPath);
            QString processedDir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + "/processed";
            QDir().mkpath(processedDir);
            QString outputPath = processedDir + "/" + QUuid::createUuid().toString(QUuid::WithoutBraces) + ".png";
            
            // 创建后台处理器
            auto processor = QSharedPointer<ImageProcessor>::create();
            m_activeImageProcessors[taskId] = processor;
            
            // 连接进度信号
            connect(processor.data(), &ImageProcessor::progressChanged,
                    this, [this, taskId](int progress, const QString&) {
                updateTaskProgress(taskId, progress);
            }, Qt::QueuedConnection);
            
            // 连接完成信号
            connect(processor.data(), &ImageProcessor::finished,
                    this, [this, taskId](bool success, const QString& resultPath, const QString& error) {
                m_activeImageProcessors.remove(taskId);
                
                if (success) {
                    completeTask(taskId, resultPath);
                } else {
                    failTask(taskId, error);
                }
            }, Qt::QueuedConnection);
            
            // 启动后台处理
            processor->processImageAsync(inputPath, outputPath, message.shaderParams);
            
            logPerfIfSlow("startTask", perfTimer.elapsed());
            return;
        }
        
        // AI 推理模式 - 现有代码
        if (message.mode == ProcessingMode::AIInference) {
            // ... 现有 AI 处理代码
        }
    }
    
    // ... 其余现有代码
}
```

#### 4. ProcessingController.cpp - 修改 completeTask() 函数

移除 Shader 图片的导出请求逻辑（因为已在 startTask 中处理）：

```cpp
void ProcessingController::completeTask(const QString& taskId, const QString& resultPath)
{
    // ... 现有代码
    
    for (const MediaFile& mf : msg.mediaFiles) {
        if (mf.id == fileId) {
            if (msg.mode == ProcessingMode::Shader && ImageUtils::isImageFile(mf.filePath)) {
                // Shader 图片已在 startTask 中处理，直接更新状态
                const QString finalPath = !resultPath.isEmpty() ? resultPath : mf.filePath;
                
                if (!finalPath.isEmpty() && ThumbnailProvider::instance()) {
                    const QString thumbId = "processed_" + fileId;
                    ThumbnailProvider::instance()->generateThumbnailAsync(finalPath, thumbId, QSize(512, 512));
                }
                
                updateFileStatusForSessionMessage(sessionId, messageId, fileId,
                    ProcessingStatus::Completed, finalPath);
                fileHandled = true;
                
                finalizeTask(taskId, sessionId, messageId);
            } else if (msg.mode == ProcessingMode::Shader && ImageUtils::isVideoFile(mf.filePath)) {
                // 视频处理 - 现有代码
                processShaderVideoThumbnailAsync(...);
                fileHandled = true;
            } else {
                // AI 推理 - 现有代码
                // ...
            }
            break;
        }
    }
    
    // ... 其余现有代码
}
```

#### 5. ProcessingController.cpp - 添加清理逻辑

在 `cleanupTask()` 和 `cancelTask()` 中添加图片处理器清理：

```cpp
void ProcessingController::cleanupTask(const QString& taskId)
{
    // 清理图片处理器
    if (m_activeImageProcessors.contains(taskId)) {
        auto processor = m_activeImageProcessors.take(taskId);
        if (processor) {
            processor->cancel();
        }
    }
    
    // ... 现有清理代码
}
```

#### 6. ProcessingController.cpp - 修改取消逻辑

在 `cancelTask()` 和 `cancelAllTasks()` 中添加图片处理取消：

```cpp
void ProcessingController::cancelTask(const QString& taskId)
{
    // ... 现有代码
    
    // 取消图片处理
    if (m_activeImageProcessors.contains(taskId)) {
        auto processor = m_activeImageProcessors.take(taskId);
        if (processor) {
            processor->cancel();
        }
    }
    
    // ... 其余现有代码
}
```

## 架构对比

### 修改前

```
Shader 图片处理：
主线程: [加载图片] → [GPU渲染] → [保存文件] → UI 卡顿

Shader 视频处理：
后台线程: [解码] → [CPU处理] → [编码] → [保存]
主线程: ← 异步进度更新 ←

AI 推理：
后台线程: [加载模型] → [推理] → [保存]
主线程: ← 异步进度更新 ←
```

### 修改后

```
Shader 图片处理：
后台线程: [加载图片] → [CPU处理] → [保存文件]
主线程: ← 异步进度更新 ←

Shader 视频处理：
后台线程: [解码] → [CPU处理] → [编码] → [保存]
主线程: ← 异步进度更新 ←

AI 推理：
后台线程: [加载模型] → [推理] → [保存]
主线程: ← 异步进度更新 ←
```

## 性能预期

1. **UI 响应性**：大幅提升，主线程不再被阻塞
2. **处理速度**：与原来相当（CPU 处理与 GPU grabToImage 性能接近）
3. **内存使用**：略有增加（后台线程需要独立的图像缓冲区）

## 兼容性说明

1. **算法一致性**：`ImageProcessor::applyShader()` 已实现与 GPU Shader 完全一致的算法
2. **输出格式**：保持 PNG 格式输出
3. **取消支持**：支持任务取消
4. **进度报告**：支持进度更新

## 风险评估

1. **低风险**：修改范围明确，仅涉及 Shader 图片处理路径
2. **可回退**：保留原有 `OffscreenShaderRenderer.qml`，可作为备选方案
3. **测试覆盖**：需要测试各种图片格式和大小

## 实施步骤

1. 修改 `ProcessingController.h` 添加必要成员
2. 修改 `ProcessingController.cpp` 构造函数
3. 修改 `startTask()` 添加 Shader 图片后台处理
4. 修改 `completeTask()` 移除重复的导出逻辑
5. 修改 `cleanupTask()` 和取消相关函数
6. 构建测试
7. 验证功能正确性和 UI 流畅性
