# Shader 图片处理 UI 卡顿问题修复笔记

## 概述

修复 Shader 模式处理图片时界面操作不流畅、卡顿的问题。

**创建日期**: 2026-03-28
**作者**: AI Assistant

---

## 一、问题描述

### 问题现象

Shader 模式正在处理图片任务时，界面操作会不流畅、有点卡顿。但 Shader 模式处理视频和 AI 推理模式处理图片/视频时不会有这种现象。

### 影响范围

- Shader 模式图片处理
- UI 响应性

---

## 二、根因分析

### 原有实现

Shader 模式处理图片时，整个处理流程都在 QML 主线程（GUI 线程）中执行：

```
ProcessingController::startTask()
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

### 对比分析

| 处理模式 | 线程模型 | UI 影响 |
|----------|----------|---------|
| Shader 图片（修复前） | 主线程 | 阻塞 UI |
| Shader 视频 | 后台线程 | 不阻塞 |
| AI 推理 | 后台线程 | 不阻塞 |

### 问题根源

Shader 模式处理图片时，图片加载、GPU 渲染、文件保存都在主线程执行，阻塞了 UI 线程。

---

## 三、解决方案

### 方案选择

将 Shader 图片处理改为与视频处理相同的后台线程模式：
- 使用 `ImageProcessor::applyShader()` 在后台线程处理
- 通过 `Qt::QueuedConnection` 异步回调主线程

### 优点

1. 与视频处理模式架构一致
2. 算法与 GPU Shader 完全一致（`ImageProcessor` 已实现）
3. 不阻塞 UI 线程
4. 代码复用度高

---

## 四、修改文件

### 1. ProcessingController.h

添加 `ImageProcessor` 头文件引用和活动处理器映射：

```cpp
#include "EnhanceVision/core/ImageProcessor.h"

// 在 private 成员中添加
QHash<QString, QSharedPointer<ImageProcessor>> m_activeImageProcessors;
```

### 2. ProcessingController.cpp - startTask()

在 `startTask()` 函数中，为 Shader 模式图片添加后台处理逻辑：

```cpp
if (message.mode == ProcessingMode::Shader) {
    // 查找输入文件路径
    QString inputPath;
    // ...
    
    if (ImageUtils::isImageFile(inputPath)) {
        // 创建后台处理器
        auto processor = QSharedPointer<ImageProcessor>::create();
        m_activeImageProcessors[taskId] = processor;
        
        // 连接进度信号（Qt::QueuedConnection）
        connect(processor.data(), &ImageProcessor::progressChanged,
                this, [this, taskId](int progress, const QString&) {
            updateTaskProgress(taskId, progress);
        }, Qt::QueuedConnection);
        
        // 连接完成信号（Qt::QueuedConnection）
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
        return;
    }
}
```

### 3. ProcessingController.cpp - completeTask()

移除 Shader 图片的导出请求逻辑（因为已在 startTask 中处理）：

```cpp
if (msg.mode == ProcessingMode::Shader && ImageUtils::isImageFile(mf.filePath)) {
    // Shader 图片已在 startTask 中处理，直接更新状态
    const QString finalPath = !resultPath.isEmpty() ? resultPath : mf.filePath;
    
    // 生成缩略图
    if (!finalPath.isEmpty() && ThumbnailProvider::instance()) {
        const QString thumbId = "processed_" + fileId;
        ThumbnailProvider::instance()->generateThumbnailAsync(finalPath, thumbId, QSize(512, 512));
    }
    
    // 更新状态
    updateFileStatusForSessionMessage(sessionId, messageId, fileId,
        ProcessingStatus::Completed, finalPath);
    fileHandled = true;
    
    finalizeTask(taskId, sessionId, messageId);
}
```

### 4. ProcessingController.cpp - cleanupTask()

添加图片处理器清理：

```cpp
void ProcessingController::cleanupTask(const QString& taskId)
{
    // ... 现有代码
    
    // 清理图片处理器
    if (m_activeImageProcessors.contains(taskId)) {
        auto processor = m_activeImageProcessors.take(taskId);
        if (processor) {
            processor->cancel();
        }
    }
    
    // ... 其余清理代码
}
```

---

## 五、架构对比

### 修改前

```
Shader 图片处理：
主线程: [加载图片] → [GPU渲染] → [保存文件] → UI 卡顿
```

### 修改后

```
Shader 图片处理：
后台线程: [加载图片] → [CPU处理] → [保存文件]
主线程: ← 异步进度更新 ←
```

---

## 六、性能影响

| 指标 | 修改前 | 修改后 | 影响 |
|------|--------|--------|------|
| UI 响应性 | 卡顿 | 流畅 | 大幅提升 |
| 处理速度 | 相当 | 相当 | 无影响 |
| 内存使用 | 低 | 略高 | 后台线程需要独立缓冲区 |

---

## 七、兼容性说明

1. **算法一致性**：`ImageProcessor::applyShader()` 已实现与 GPU Shader 完全一致的算法
2. **输出格式**：保持 PNG 格式输出
3. **取消支持**：支持任务取消
4. **进度报告**：支持进度更新

---

## 八、测试验证

| 测试场景 | 预期结果 | 实际结果 |
|----------|----------|----------|
| Shader 图片处理时拖动窗口 | 流畅 | ✅ 通过 |
| Shader 图片处理时点击按钮 | 响应及时 | ✅ 通过 |
| 多张图片连续处理 | 正常完成 | ✅ 通过 |
| 处理过程中取消 | 正确取消 | ✅ 通过 |
| 处理结果与预览一致 | 效果一致 | ✅ 通过 |

---

## 九、后续优化建议

1. 可考虑添加处理进度百分比显示
2. 可考虑支持更多图片格式输出选项
3. 可考虑添加处理速度统计
