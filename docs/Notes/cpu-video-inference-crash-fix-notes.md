# CPU 视频 AI 推理崩溃修复笔记

## 概述
**创建日期**: 2026-04-03  
**最后更新**: 2026-04-03  
**状态**: ✅ 已解决  
**问题描述**: 
1. AI 视频推理在 CPU 模式下出现堆损坏崩溃（Windows 异常 0xc0000374）
2. 取消/删除正在运行的 AI 推理任务后程序闪退（Use-After-Free 崩溃）

---

## 一、问题分析

### 崩溃现象
- 崩溃发生在视频帧处理过程中
- Windows 报告堆损坏异常 (0xc0000374)
- 崩溃位置不固定，可能在 QImage 创建、NCNN 推理、或内存拷贝时触发
- 崩溃是累积性的，通常在处理几帧后发生
- **特别是竖向视频（如 360x640）更容易触发崩溃**

### 根本原因分析
1. **FFmpeg 缓冲区对齐问题**: `sws_scale` 需要正确对齐的目标缓冲区，直接写入 QImage 内部缓冲区会导致越界写入
2. **内存对齐问题**: QImage 的 `bytesPerLine()` 可能与 FFmpeg/NCNN 期望的 stride 不匹配
3. **隐式共享问题**: QImage 的隐式共享机制在多线程环境下可能导致数据竞争
4. **NCNN 内存管理**: NCNN 的 `from_pixels` 和 `to_pixels` 函数对 stride 处理敏感

---

## 二、已尝试的修复方案

### 方案 1: FFmpeg 缓冲区对齐（失败）
- 使用 `av_image_get_buffer_size` 和 `av_image_fill_arrays` 分配对齐缓冲区
- 结果：仍然崩溃

### 方案 2: QImage 深拷贝（失败）
- 使用 `QImage::copy()` 和 `detach()` 确保数据独立
- 结果：仍然崩溃

### 方案 3: AIEngine 边界检查（失败）
- 在 `processTiled` 中添加显式边界检查
- 结果：仍然崩溃

### 方案 4: NCNN 配置优化（失败）
- 禁用 `use_packing_layout`
- 启用 `lightmode`
- 限制线程数
- 结果：仍然崩溃

### 方案 5: 强制使用 processSingle（部分成功）
- 跳过分块处理，直接使用单次推理
- 结果：程序可以处理更多帧（3帧），但仍然崩溃

### 方案 6: FFmpeg av_image 函数 + 正确的内存对齐（部分成功）
- 使用 `av_image_get_buffer_size` 计算正确的缓冲区大小
- 使用 `av_malloc` 分配 32 字节对齐的内存
- 使用 `av_image_fill_arrays` 设置正确的指针和 stride
- 逐行拷贝数据到 QImage，只拷贝有效像素数据
- NCNN 的 `qimageToMat` 和 `matToQimage` 使用连续内存缓冲区
- 结果：**部分成功，仍有问题**

### 方案 7: AVFrame 作为中间缓冲区（✅ 最终解决方案）
- 使用 `AVFrame` 作为 sws_scale 的目标缓冲区
- 让 FFmpeg 完全管理内存分配，避免手动内存管理问题
- 使用 `av_frame_get_buffer()` 分配对齐的缓冲区
- 手动逐行拷贝到 QImage，只拷贝有效像素数据
- NCNN 的 `matToQimage` 使用 32 字节对齐的 stride
- 结果：**✅ 成功解决**

---

## 三、最终实施的修复

### 3.1 AIVideoProcessor.cpp - 视频帧处理（关键修复）

```cpp
// 【关键修复】使用 AVFrame 作为中间缓冲区，让 FFmpeg 自己管理内存
// 这是最安全的方式，避免手动内存管理导致的问题
AVFrame* rgbFrame = av_frame_alloc();
if (!rgbFrame) {
    qWarning() << "Failed to allocate RGB frame";
    av_frame_unref(decFrame);
    continue;
}

rgbFrame->format = AV_PIX_FMT_RGB24;
rgbFrame->width = frameW;
rgbFrame->height = frameH;

// 让 FFmpeg 分配缓冲区
int ret = av_frame_get_buffer(rgbFrame, 32);  // 32 字节对齐
if (ret < 0) {
    qWarning() << "Failed to allocate RGB frame buffer";
    av_frame_free(&rgbFrame);
    av_frame_unref(decFrame);
    continue;
}

sws_scale(decSwsCtx, decFrame->data, decFrame->linesize,
           0, frameH, rgbFrame->data, rgbFrame->linesize);

// 直接创建目标 QImage 并手动拷贝数据
QImage processedFrame(frameW, frameH, QImage::Format_RGB888);

// 逐行拷贝数据到 QImage
const int copyBytes = frameW * 3;  // RGB888 每像素 3 字节
for (int y = 0; y < frameH; ++y) {
    std::memcpy(processedFrame.scanLine(y), 
               rgbFrame->data[0] + y * rgbFrame->linesize[0], 
               copyBytes);
}

// 释放 RGB 帧
av_frame_free(&rgbFrame);
```

### 3.2 AIEngine.cpp - NCNN 配置

```cpp
// 使用单线程推理，禁用可能导致内存问题的优化
m_opt.num_threads = 1;
m_opt.use_packing_layout = false;
m_opt.use_vulkan_compute = false;
m_opt.lightmode = false;
m_opt.use_local_pool_allocator = false;
m_opt.use_sgemm_convolution = false;
m_opt.use_winograd_convolution = false;
```

### 3.3 AIEngine.cpp - qimageToMat 修复

```cpp
// 如果 QImage 的 bytesPerLine 与紧凑排列不同，先拷贝到连续内存缓冲区
if (srcStride != dstStride) {
    buffer.resize(static_cast<size_t>(w) * h * 3);
    for (int y = 0; y < h; ++y) {
        const uchar* srcLine = img.constScanLine(y);
        std::memcpy(buffer.data() + y * dstStride, srcLine, dstStride);
    }
    pixelData = buffer.data();
    actualStride = dstStride;
}
```

### 3.4 AIEngine.cpp - matToQimage 修复（关键修复）

```cpp
// 【关键修复】使用 32 字节对齐的 stride，与 FFmpeg 保持一致
const int compactStride = w * 3;  // RGB888 紧凑排列
const int alignedStride = (compactStride + 31) & ~31;  // 32 字节对齐
const size_t bufferSize = static_cast<size_t>(alignedStride) * h;
std::vector<unsigned char> buffer(bufferSize);

// 使用对齐的 stride 调用 to_pixels
out.to_pixels(buffer.data(), ncnn::Mat::PIXEL_RGB, alignedStride);

QImage result(w, h, QImage::Format_RGB888);
// 逐行拷贝到 QImage（从对齐的缓冲区拷贝紧凑的像素数据）
for (int y = 0; y < h; ++y) {
    std::memcpy(result.scanLine(y), buffer.data() + y * alignedStride, compactStride);
}
```

---

## 四、开发注意事项

### 4.1 QImage 使用注意事项

1. **避免直接写入 QImage 内部缓冲区**
   - QImage 的 `bytesPerLine()` 可能包含填充字节
   - 外部库（如 FFmpeg、NCNN）可能不正确处理 stride

2. **使用深拷贝避免隐式共享问题**
   - 在多线程环境下，使用 `QImage::copy()` 确保数据独立
   - 避免在不同线程间共享 QImage 引用

3. **格式转换**
   - 始终使用 `QImage::Format_RGB888` 与 NCNN 交互
   - 使用 `convertToFormat()` 前检查源格式

### 4.2 NCNN 使用注意事项

1. **stride 处理**
   - `ncnn::Mat::from_pixels` 需要正确的 stride 参数
   - 建议使用紧凑排列的缓冲区（stride = width * channels）

2. **CPU 模式配置**
   - 禁用 `use_packing_layout` 可能提高稳定性
   - 单线程推理更稳定但更慢

3. **内存管理**
   - NCNN Mat 的 `clone()` 会创建独立副本
   - 注意 extractor 的生命周期

### 4.3 FFmpeg 使用注意事项

1. **sws_scale 目标缓冲区**
   - **最佳实践**：使用 `AVFrame` 作为目标缓冲区，让 FFmpeg 管理内存
   - 使用 `av_frame_get_buffer()` 分配对齐的缓冲区
   - 避免直接写入 QImage 内部缓冲区

2. **资源释放**
   - 始终调用 `av_frame_unref()` 释放解码帧
   - 使用 `av_frame_free()` 释放分配的帧
   - 使用 `sws_freeContext()` 释放转换上下文

3. **内存对齐**
   - 使用 32 字节对齐确保 SIMD 优化兼容性
   - FFmpeg 会自动处理对齐，无需手动计算

### 4.4 调试技巧

1. **添加详细日志**
   - 在关键操作前后添加日志
   - 使用 `fflush(stdout)` 确保日志立即输出

2. **逐步排查**
   - 使用 `processSingle` 跳过分块处理
   - 逐步启用/禁用功能定位问题

3. **内存检查**
   - 使用 Windows Application Verifier 检测堆损坏
   - 检查所有内存分配和释放

---

## 五、支持的视频尺寸

修复后，程序能够智能处理各种视频尺寸和比例：

| 视频类型 | 示例尺寸 | 处理方式 | 状态 |
|----------|----------|----------|------|
| 横向视频 | 640x288, 1920x1080 | processSingle 或 processTiled | ✅ 正常 |
| 竖向视频 | 360x640, 1080x1920 | processSingle 或 processTiled | ✅ 正常 |
| 正方形视频 | 720x720, 1080x1080 | processSingle 或 processTiled | ✅ 正常 |
| 小尺寸视频 | 320x240, 480x360 | processSingle | ✅ 正常 |
| 大尺寸视频 | 1920x1080, 4K | processTiled | ⚠️ 暂时禁用 |

### 智能处理逻辑

1. **自动选择处理模式**
   - 小图像（尺寸小于 tileSize）：使用 `processSingle`
   - 大图像（尺寸大于 tileSize）：使用 `processTiled`（暂时强制使用 `processSingle`）

2. **动态缓冲区分配**
   - 使用 `AVFrame` 让 FFmpeg 自动管理内存
   - 32 字节对齐确保 SIMD 优化兼容性

3. **无硬编码限制**
   - 所有尺寸计算基于输入视频的实际尺寸
   - 支持任意宽高比

### 当前限制

- **分块处理暂时禁用**：`processTiled` 仍有内存问题，临时强制使用 `processSingle`
- **性能影响**：大尺寸视频处理速度较慢，因为无法使用分块优化

---

## 六、性能影响

为稳定性付出的代价：
- 单线程推理速度较慢（约 7 秒/帧 for 360x640）
- 禁用部分 NCNN 优化选项

后续可考虑的优化：
- 逐步启用 NCNN 优化选项测试稳定性
- 增加线程数（需要验证稳定性）

---

## 七、待解决问题

1. **分块处理 (processTiled) 仍有问题**
   - 当前强制使用 `processSingle` 作为临时解决方案
   - `processTiled` 中使用 `QImage::copy()` 可能在堆损坏时触发崩溃
   - 需要对 `processTiled` 应用类似的内存管理修复

2. **性能影响**
   - 单线程推理速度较慢（约 7 秒/帧 for 360x640）
   - 禁用部分 NCNN 优化选项
   - 大尺寸视频无法使用分块优化

3. **长期稳定性**
   - 需要更多测试验证修复效果
   - 可能需要监控内存使用情况

---

## 八、相关文件

| 文件路径 | 修改内容 |
|----------|----------|
| `src/core/AIEngine.cpp` | NCNN 配置、qimageToMat、matToQimage（32字节对齐）、强制 processSingle |
| `src/core/video/AIVideoProcessor.cpp` | AVFrame 缓冲区管理、手动逐行拷贝、定期清理缓存 |
| `include/EnhanceVision/core/AIEngine.h` | 添加 clearInferenceCache 声明 |

---

## 九、参考资料

- [NCNN Wiki - use ncnn with opencv](https://github.com/Tencent/ncnn/wiki/use-ncnn-with-opencv)
- [Qt QImage Documentation](https://doc.qt.io/qt-6/qimage.html)
- [FFmpeg sws_scale Documentation](https://ffmpeg.org/doxygen/trunk/group__libsws.html)
- [FFmpeg av_image_fill_arrays](https://ffmpeg.org/doxygen/trunk/group__lavu__picture.html)
- [FFmpeg av_frame_get_buffer](https://ffmpeg.org/doxygen/trunk/group__lavu__frame.html)

---

## 十、取消 AI 推理任务崩溃修复

### 10.1 问题现象

在 AI 推理模式下处理视频时，用户取消/删除正在运行的任务后，程序界面闪退（崩溃）。

**关联问题**：
1. 取消任务时界面严重卡顿约 2 秒，甚至无响应
2. 多任务场景下删除一个任务导致其他任务被级联删除
3. 删除一个任务后，下一个任务处理失败（奇偶规律：第2个失败、第3个成功...）
4. 删除第一个任务后，"处理中"状态跳转到第三个任务缩略图上
5. 删除按钮无效（点击缩略图删除按钮无反应）

### 10.2 根因分析

#### 崩溃时序（通过 runtime_output.log 确认）

```
时间轴：
[03:26:14.115] Frame 1 开始 AI 推理（每帧约需 9 秒）
[03:26:17.588] ← 用户点击取消
[03:26:17.588] Cancel requested
[03:26:17.691] Waiting for processing to finish, timeout: 2000 ms
[03:26:19.700] ⚠️ Wait timeout, processing still running  ← 关键！超时了！
[03:26:19.700] Task released: "10710e4e-..."                ← 引擎被强制释放
[03:26:22.918] Inference complete...                        ← 推理在已释放的资源上完成 → 💥崩溃
```

#### 根本原因：AIVideoProcessor 对象生命周期管理缺陷

**调用链路**：

```
用户点击取消
  → ProcessingController::gracefulCancel(taskId)
    → ProcessingController::cleanupTask(taskId)
      → aiProcessor = m_activeAIVideoProcessors.take(taskId)
         ⚠️ take() 移除了 map 中最后一个 QSharedPointer 引用
      → aiProcessor->cancel()
      → aiProcessor->waitForFinished(2000)            超时 2 秒
      → }  ← if 块结束，局部变量 aiProcessor 离开作用域
         💥 QSharedPointer 引用计数归零 → AIVideoProcessor 被删除！
      → m_aiEnginePool->release(taskId)               引擎被释放
```

**核心问题**：`waitForFinished(2000)` 超时后，代码假设任务已结束并继续清理，但实际 `processInternal()` 仍在 QtConcurrent 线程中运行。此时发生以下致命问题：

| # | 问题 | 后果 |
|---|------|------|
| 1 | **AIVideoProcessor 被 delete**（引用计数归零） | `processInternal()` 访问 `m_impl` → **Use-After-Free 崩溃** |
| 2 | **AIEnginePool 提前 release** | 引擎状态重置为 Ready，可能被新任务获取；原推理线程仍在使用该引擎 → **数据竞争/崩溃** |
| 3 | **信号槽已断开** | `processInternal()` 完成后 emit `completed()` → 发送到已断开/已删除的对象 |

#### 其他关联问题的根因

| 问题 | 根因 |
|------|------|
| 界面卡顿 | `waitForFinished(2000)` 阻塞主线程 + 同步资源清理 |
| 级联误删 | `cancelTask()` 有 messageId 自动检测逻辑 + `fileRemoved` 信号重复触发 |
| 后续任务失败 | `engine->cancelProcess()` 从 5 个位置被调用，污染引擎内部 NCNN 状态 |
| 状态跳转 | 已删除任务的 `completed` 信号延迟到达时，重复 release 引擎 + 调用 processNextTask |
| 删除按钮无效 | QML 中 `root.messageId` 属性名错误（应为 `root.taskId`），导致 `messageModel.removeMediaFile(undefined, index)` 无法执行；MouseArea 事件拦截问题 |

### 10.3 修复方案

#### 修复 1：cleanupTask() 异步化（解决崩溃和卡顿）

**文件**: `src/controllers/ProcessingController.cpp`

```cpp
void ProcessingController::cleanupTask(const QString& taskId)
{
    TaskStateManager::instance()->unregisterActiveTask(taskId);
    cancelVideoProcessing(taskId);

    // 阶段 1：仅发送取消请求，不 take 不释放
    if (m_activeAIVideoProcessors.contains(taskId)) {
        auto processor = m_activeAIVideoProcessors.value(taskId);
        if (processor) {
            processor->cancel();  // 仅设置取消标志
        }
    }

    // 图片处理器可以同步清理（处理速度快）
    QSharedPointer<ImageProcessor> imgProcessor;
    if (m_activeImageProcessors.contains(taskId)) {
        imgProcessor = m_activeImageProcessors.take(taskId);
        if (imgProcessor) {
            imgProcessor->cancel();
        }
    }

    // 阶段 2：所有重量级操作通过 QTimer::singleShot(0) 延迟到下一事件循环
    QTimer::singleShot(0, this, [this, taskId]() {
        disconnectAiEngineForTask(taskId);
        m_aiEnginePool->release(taskId);
        m_resourceManager->release(taskId);
        m_taskCoordinator->unregisterTask(taskId);
        // ... 其他清理操作

        // 对 AI 视频处理任务，设置兜底清理定时器
        if (m_activeAIVideoProcessors.contains(taskId)) {
            QTimer::singleShot(15000, this, [this, taskId]() {
                handleOrphanedVideoTask(taskId);  // 15秒兜底强制清理
            });
        }
    });
}
```

#### 修复 2：handleOrphanedVideoTask() 兜底清理

**文件**: `src/controllers/ProcessingController.cpp`

```cpp
void ProcessingController::handleOrphanedVideoTask(const QString& taskId)
{
    if (!m_activeAIVideoProcessors.contains(taskId)) {
        return;  // 已被正常清理
    }

    qWarning() << "[ProcessingController] Orphaned video task cleanup:" << taskId;

    auto processor = m_activeAIVideoProcessors.take(taskId);
    if (processor) {
        processor->cancel();
        processor->waitForFinished(5000);  // 最后等待 5 秒
    }

    disconnectAiEngineForTask(taskId);
    m_aiEnginePool->release(taskId);
    adjustProcessingCount(-1);

    qInfo() << "[ProcessingController] Orphaned video task cleanup completed:" << taskId;
}
```

#### 修复 3：移除所有 engine->cancelProcess() 调用

**涉及文件**：
- `src/core/video/AIVideoProcessor.cpp`
- `src/core/AIEnginePool.cpp`
- `src/core/ai/AIInferenceWorker.cpp`
- `src/core/ProcessingEngine.cpp`

**原因**：`cancelProcess()` 会破坏引擎内部 NCNN 推理上下文状态，导致后续任务失败。

**修复**：
- 移除所有 `engine->cancelProcess()` 调用
- `AIEnginePool::acquire()` 改用 `resetState()` 重置引擎状态

#### 修复 4：completed 信号槽防重复清理

**文件**: `src/controllers/ProcessingController.cpp` 的 `launchAIVideoProcessor()`

```cpp
connect(processor.data(), &AIVideoProcessor::completed,
        this, [this, taskId](bool success, const QString& result, const QString& error) {
    // 检查任务是否已被移除
    if (!m_activeAIVideoProcessors.contains(taskId)) { return; }
    
    bool taskAlreadyRemoved = true;
    for (const auto& t : m_tasks) {
        if (t.taskId == taskId) { taskAlreadyRemoved = false; break; }
    }

    m_activeAIVideoProcessors.remove(taskId);
    disconnectAiEngineForTask(taskId);
    
    if (!taskAlreadyRemoved) {
        m_aiEnginePool->release(taskId);
        if (success) { completeTask(taskId, result); }
        else { failTask(taskId, error); }
    }
}, Qt::QueuedConnection);
```

#### 修复 5：cancelTask() 精确匹配

**文件**: `src/controllers/ProcessingController.cpp`

移除 messageId 自动检测逻辑，仅按 taskId 精确匹配取消单个任务。

#### 修复 6：MessageModel::removeMediaFile() 异步化

**文件**: `src/models/MessageModel.cpp`

```cpp
bool MessageModel::removeMediaFile(const QString &messageId, int fileIndex)
{
    // ... 参数验证
    const QString removedFileId = message.mediaFiles[fileIndex].id;

    QTimer::singleShot(0, this, [this, messageId, fileIndex, removedFileId]() {
        if (m_processingController && !removedFileId.isEmpty()) {
            m_processingController->cancelMessageFileTasks(messageId, removedFileId);
        }
        // ... 实际删除操作
    });

    return true;
}
```

#### 修复 7：QML 层异步调用

**文件**: `qml/components/MessageItem.qml`

```qml
// 修复前（.bind() 兼容性问题）
onDeleteFile: function(index) { Qt.callLater(messageModel.removeMediaFile.bind(messageModel, root.messageId, index)) }

// 修复后（闭包捕获参数）
onDeleteFile: function(index) { var _msgId = root.taskId; var _idx = index; Qt.callLater(function() { messageModel.removeMediaFile(_msgId, _idx) }) }
```

**注意**：`MessageItem.qml` 中的属性是 `taskId`，不是 `messageId`。使用错误的属性名会导致 `undefined` 参数传递给 `removeMediaFile()`。

#### 修复 8：移除冗余 fileRemoved 信号

**文件**: `qml/components/MediaThumbnailStrip.qml`

移除 4 处 `root.fileRemoved(root.messageId, origIndex)` 调用，避免信号级联导致重复删除。

#### 修复 9：删除按钮 MouseArea 事件处理

**文件**: `qml/components/MediaThumbnailStrip.qml`

1. 为 4 处删除按钮的 MouseArea 添加 `preventStealing: true`：
   - `horizontalComponent - deleteBtnMouse`
   - `horizontalComponent - deleteBtnForFailedMouse`
   - `gridComponent - deleteBtnMouse`
   - `gridComponent - deleteBtnForFailedMouse`

2. 为 `thumbMouse` 添加事件传播：

```qml
MouseArea {
    id: thumbMouse
    propagateComposedEvents: true
    onPressed: function(mouse) {
        var deleteBtnArea = Qt.rect(parent.width - 24, 4, 20, 20)
        var inDeleteBtn = mouse.x >= deleteBtnArea.x && mouse.x <= deleteBtnArea.x + deleteBtnArea.width &&
                          mouse.y >= deleteBtnArea.y && mouse.y <= deleteBtnArea.y + deleteBtnArea.height
        if (inDeleteBtn && thumbDelegate.showDeleteBtn) {
            mouse.accepted = false  // 让事件传播到删除按钮
        }
    }
    // ...
}
```

### 10.4 相关文件

| 文件路径 | 修改内容 |
|----------|----------|
| `src/controllers/ProcessingController.cpp` | cleanupTask 异步化、handleOrphanedVideoTask 兜底、cancelTask 精确匹配、completed 防重复 |
| `src/controllers/ProcessingController.h` | 新增 handleOrphanedVideoTask 声明 |
| `src/core/video/AIVideoProcessor.cpp` | 移除 cancel() 中的 engine->cancelProcess() |
| `src/core/AIEnginePool.cpp` | release() 不调用 cancelProcess，acquire() 使用 resetState |
| `src/core/ai/AIInferenceWorker.cpp` | 移除 cancel() 中的 engine->cancelProcess() |
| `src/core/ProcessingEngine.cpp` | 移除 cancelTaskByNodeId 中的 engine->cancelProcess() |
| `src/models/MessageModel.cpp` | removeMediaFile 异步化 |
| `qml/components/MessageItem.qml` | onDeleteFile 闭包修复，`root.messageId` → `root.taskId` |
| `qml/components/MediaThumbnailStrip.qml` | 移除 4 处 fileRemoved 信号发射；删除按钮 MouseArea 添加 `preventStealing: true`；thumbMouse 添加事件传播 |

### 10.5 验证步骤

1. 启动程序，开始一个 AI 视频处理任务（使用较长的视频）
2. 在处理过程中（至少等第一帧推理开始后）点击取消/删除任务
3. **预期结果**：
   - ✅ 任务正常取消，程序不崩溃
   - ✅ UI 无卡顿（点击瞬间响应流畅）
   - ✅ 多任务场景下删除一个不影响其他任务
   - ✅ 后续任务能正常处理完成
   - ✅ 缩略图删除按钮可正常点击删除单个文件
4. 检查 `runtime_output.log`：无崩溃堆栈、无警告
5. 连续多次取消→重新处理，验证无资源泄漏

### 10.6 关键经验总结

#### QSharedPointer 与异步线程生命周期

1. **不要在异步任务运行时 take() 最后一个引用**
   - `take()` 会移除 map 中的 QSharedPointer
   - 如果局部变量是最后一个引用，离开作用域后对象被删除
   - 异步线程仍在运行 → Use-After-Free

2. **正确做法**
   - 保持引用在 map 中，直到确认异步任务完成
   - 使用信号槽（completed）作为唯一清理入口
   - 设置兜底定时器防止资源泄漏

#### Qt 信号槽与多线程

1. **QueuedConnection 的时序问题**
   - 信号可能延迟到达
   - 需要检查任务是否已被清理
   - 防止重复处理

2. **引擎状态管理**
   - 不要从多个位置调用状态修改方法
   - 集中管理状态重置
   - 使用 resetState 而非 cancelProcess

#### QML/JS 交互注意事项

1. **`.bind()` 兼容性问题**
   - Qt QML JS 引擎对 `.bind()` 支持有限
   - 与 `Qt.callLater()` 组合时可能丢失上下文
   - 推荐使用闭包捕获参数

2. **信号级联风险**
   - 避免一个操作触发多个信号
   - 检查信号处理器是否会导致重复操作
   - 移除冗余信号发射

3. **属性名正确性**
   - 确保 QML 中引用的属性名与组件定义一致
   - `MessageItem.qml` 使用 `taskId` 而非 `messageId`
   - 使用未定义属性会得到 `undefined`，导致静默失败

4. **MouseArea 事件传播**
   - 子元素 MouseArea 可能被父元素 MouseArea 拦截
   - 使用 `preventStealing: true` 防止事件被偷取
   - 使用 `propagateComposedEvents: true` 配合 `mouse.accepted = false` 传播事件

---

## 十一、快速连续删除任务崩溃修复（2026-04-03）

### 11.1 问题现象

在 Shader 模式和 AI 推理模式下，高压连续快速点击消息卡片中缩略图右上角删除按钮时：
1. **Shader 模式**：出现闪退
2. **AI 推理模式**：出现崩溃/界面无响应状态
3. **AI 推理模式**：删除一个消息卡片中的所有任务后，新的消息卡片任务变成等待状态（残留任务未被清除）

### 11.2 根因分析

#### Shader 模式闪退根因

`ImageProcessor` 的 `finished` 回调未捕获 `processor` 共享指针。当 `terminateTask` 从 `m_activeImageProcessors` 中移除处理器后，若无其他引用持有，处理器被销毁，但其后台线程仍在运行 → **Use-After-Free 崩溃**。

```cpp
// 问题代码：lambda 未捕获 processor
connect(processor.data(), &ImageProcessor::finished,
        this, [this, taskId](bool success, ...) {  // ❌ 未捕获 processor
    m_activeImageProcessors.remove(taskId);
    // ...
}, Qt::QueuedConnection);
```

#### AI 推理模式崩溃/无响应根因

1. **`AIEngine::cancelProcess()` 立即设置 `m_isProcessing = false`**：但后台线程中的 `process()` 调用可能仍在执行 NCNN 推理。这导致 `isProcessing()` 返回 false，但实际推理仍在进行。

2. **`AIEnginePool::release()` 不检查引擎状态**：直接标记为 `Ready`。下一个任务获取到仍在处理中的引擎 → **并发访问/引擎损坏/卡死**。

3. **`processDeletionBatch` 只对 Draining 状态释放引擎**：Canceling/Cleaning 状态的引擎释放被延迟到异步回调，但 `processNextTask` 可能在引擎释放前就被调用。

### 11.3 修复方案

#### 修复 1：ImageProcessor 回调捕获 shared_ptr

**文件**: `src/controllers/ProcessingController.cpp`

```cpp
// 捕获 processor 共享指针，防止 terminateTask 移除后 use-after-free
connect(processor.data(), &ImageProcessor::progressChanged,
        this, [this, taskId, processor](int progress, const QString&) {
    Q_UNUSED(processor);  // 保持引用计数
    updateTaskProgress(taskId, progress);
}, Qt::QueuedConnection);

connect(processor.data(), &ImageProcessor::finished,
        this, [this, taskId, processor](bool success, const QString& resultPath, const QString& error) {
    Q_UNUSED(processor);  // 保持引用计数直到回调完成
    m_activeImageProcessors.remove(taskId);
    // ...
}, Qt::QueuedConnection);
```

#### 修复 2：AIEngine::cancelProcess() 不立即设置 isProcessing

**文件**: `src/core/AIEngine.cpp`

```cpp
void AIEngine::cancelProcess()
{
    qInfo() << "[AIEngine] cancelProcess() called, isProcessing:" << m_isProcessing.load();
    m_cancelRequested = true;
    // 注意：不要在这里设置 m_isProcessing = false
    // 让后台线程在检测到取消后自己设置，确保推理真正停止
}

void AIEngine::forceCancel()
{
    m_forceCancelled = true;
    m_cancelRequested = true;
    // 注意：不要在这里设置 m_isProcessing = false
    qWarning() << "[AIEngine] Force cancel requested";
}
```

#### 修复 3：AIEnginePool::acquire() 跳过正在处理的引擎

**文件**: `src/core/AIEnginePool.cpp`

```cpp
AIEngine* AIEnginePool::acquire(const QString& taskId)
{
    // ...
    for (int i = 0; i < m_slots.size(); ++i) {
        // ...
        // 确保引擎不在处理中（防止上一个任务取消后引擎仍在运行）
        AIEngine* engine = m_slots[i].engine;
        if (engine->isProcessing()) {
            qWarning() << "[AIEnginePool] Engine slot" << i << "still processing, skipping (will retry later)";
            // 不阻塞等待，直接跳过这个引擎，让调用方稍后重试
            continue;
        }
        // ...
    }
}
```

#### 修复 4：AIEnginePool::release() 取消引擎处理

**文件**: `src/core/AIEnginePool.cpp`

```cpp
void AIEnginePool::release(const QString& taskId)
{
    // ...
    AIEngine* engine = m_slots[idx].engine;
    
    // 释放前取消引擎处理，确保下次获取时引擎空闲
    if (engine && engine->isProcessing()) {
        qInfo() << "[AIEnginePool] Cancelling engine before release, task:" << taskId;
        engine->cancelProcess();
    }
    // ...
}
```

#### 修复 5：ProcessingController 引擎池暂时耗尽时延迟重试

**文件**: `src/controllers/ProcessingController.cpp`

```cpp
// 从引擎池获取可用引擎（可能因引擎仍在处理而暂时不可用）
AIEngine* engine = m_aiEnginePool->acquire(taskId);
if (!engine) {
    // 引擎池暂时耗尽，延迟重试而不是立即失败
    qInfo() << "[ProcessingController] AI engine pool temporarily exhausted for task:" << taskId
            << ", will retry in 500ms";
    // 将任务状态改回 Pending，稍后重试
    for (auto& t : m_tasks) {
        if (t.taskId == taskId) {
            t.status = ProcessingStatus::Pending;
            break;
        }
    }
    adjustProcessingCount(-1);
    QTimer::singleShot(500, this, &ProcessingController::processNextTask);
    return;
}
```

#### 修复 6：terminateTask 取消 AI 引擎

**文件**: `src/controllers/ProcessingController.cpp`

```cpp
m_taskLifecycle[taskId] = TaskLifecycle::Canceling;

// 取消 AI 引擎处理（在断开连接前，确保引擎停止）
AIEngine* engine = m_aiEnginePool->engineForTask(taskId);
if (engine && engine->isProcessing()) {
    qInfo() << "[ProcessingController] Cancelling AI engine for task:" << taskId;
    engine->cancelProcess();
}

disconnectAiEngineForTask(taskId);
```

#### 修复 7：processDeletionBatch 立即释放所有被终止任务的引擎

**文件**: `src/controllers/ProcessingController.cpp`

```cpp
// 对所有被终止的任务立即释放引擎和资源
// （不仅是 Draining，Canceling/Cleaning 的异步释放可能来不及在 processNextTask 前完成）
for (const auto& tt : tasksToTerminate) {
    TaskLifecycle lifecycle = m_taskLifecycle.value(tt.taskId, TaskLifecycle::Active);
    if (lifecycle == TaskLifecycle::Draining) {
        m_dyingProcessors.remove(tt.taskId);
        m_dyingVideoProcessors.remove(tt.taskId);
    }
    // 所有被终止的任务都立即释放引擎
    m_aiEnginePool->release(tt.taskId);
    releaseTaskResources(tt.taskId);
    m_taskLifecycle[tt.taskId] = TaskLifecycle::Dead;
}
```

### 11.4 相关文件

| 文件路径 | 修改内容 |
|----------|----------|
| `src/controllers/ProcessingController.cpp` | ImageProcessor 回调捕获 shared_ptr；terminateTask 取消 AI 引擎；processDeletionBatch 立即释放所有引擎；引擎池耗尽时延迟重试 |
| `src/core/AIEngine.cpp` | cancelProcess/forceCancel 不立即设置 isProcessing=false |
| `src/core/AIEnginePool.cpp` | acquire 跳过正在处理的引擎；release 取消引擎处理 |
| `src/models/MessageModel.cpp` | removeMediaFile 同步执行+仅文件级防抖 |
| `qml/components/MessageItem.qml` | fileId 级防抖替代消息级锁 |

### 11.5 验证步骤

1. **Shader 模式**：快速连续点击不同缩略图的删除按钮
   - ✅ 预期：无闪退，UI 流畅响应

2. **AI 推理模式**：快速连续点击不同缩略图的删除按钮
   - ✅ 预期：无崩溃/无响应，UI 流畅响应

3. **AI 推理模式**：删除一个消息卡片中的所有任务后，发送新的消息卡片任务
   - ✅ 预期：新任务正常处理，不会卡在等待状态

### 11.6 关键经验总结

1. **Lambda 捕获共享指针**：当异步回调可能在对象被移除后执行时，必须在 lambda 中捕获共享指针以保持引用计数。

2. **取消标志 vs 状态标志**：`cancelProcess()` 应该只设置取消标志，让后台线程自己在检测到取消后设置状态标志，确保状态与实际执行一致。

3. **非阻塞资源获取**：当资源暂时不可用时，应该延迟重试而不是阻塞等待或立即失败，避免 UI 卡顿。

4. **立即释放 vs 延迟释放**：在批量删除场景中，应该立即释放所有被终止任务的资源，而不是依赖异步回调，确保后续任务能获取到空闲资源。
