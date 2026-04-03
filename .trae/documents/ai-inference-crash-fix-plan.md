# AI 推理崩溃问题修复计划

## 问题概述

AI 推理模式在处理视频或图像时，应用程序会崩溃闪退。崩溃发生在 `tempImg.copy()` 调用期间或之后。

## 崩溃现场分析

### 日志关键信息

```
[2026-04-02 10:47:29.088] [INFO] [AIVideoProcessor][DEBUG] Frame 0 tempImg created 
    size: 360 x 640 format: QImage::Format_RGB888 isNull: false 
    bytesPerLine: 1080 sizeInBytes: 691200 bits: valid
[2026-04-02 10:47:29.088] [INFO] [AIVideoProcessor][DEBUG] Frame 0 calling tempImg.copy()
```

**关键观察**：
- `tempImg` 创建成功，所有属性正常
- `bits()` 返回有效指针
- `sizeInBytes` = 691200 (360 × 640 × 3 = 691200) ✅ 正确
- `bytesPerLine` = 1080 (360 × 3 = 1080) ✅ 正确
- 崩溃发生在 `tempImg.copy()` 调用后

---

## 根本原因分析

### 🔴 问题 1：堆损坏（最可能的原因）

**位置**: `AIVideoProcessor.cpp:428-455`

```cpp
// 第 414 行：创建局部缓冲区
std::vector<uint8_t> rgb24Buffer(rgb24LineSize * frameH);

// 第 428 行：QImage 引用外部数据
QImage tempImg(rgb24Buffer.data(), frameW, frameH, rgb24LineSize, QImage::Format_RGB888);

// 第 455 行：深拷贝
QImage frameImg = tempImg.copy();  // 💥 崩溃点
```

**分析**：
- `tempImg.copy()` 本身是安全的操作
- 如果崩溃发生在 `copy()` 之后，说明**堆已经被之前的操作损坏**
- `copy()` 触发了堆内存分配，暴露了已存在的堆损坏

**可能的堆损坏来源**：

1. **Vulkan Allocator 状态问题**
   - GPU 命令队列中仍有未完成的操作
   - Vulkan allocator 内部状态不一致

2. **FFmpeg 资源管理问题**
   - `sws_scale()` 可能写入越界
   - `AVFrame` 生命周期管理

### 🔴 问题 2：sws_scale 潜在越界

**位置**: `AIVideoProcessor.cpp:421-422`

```cpp
std::vector<uint8_t> rgb24Buffer(rgb24LineSize * frameH);
uint8_t* dst[4] = { rgb24Buffer.data(), nullptr, nullptr, nullptr };
int dstStride[4] = { rgb24LineSize, 0, 0, 0 };

sws_scale(decSwsCtx, decFrame->data, decFrame->linesize,
          0, frameH, dst, dstStride);
```

**风险点**：
- `rgb24LineSize = frameW * 3` 假设紧密排列
- 如果 `sws_scale` 内部对输出行有对齐要求，可能越界写入
- 某些像素格式转换可能需要额外的缓冲空间

### 🔴 问题 3：QImage 构造函数的内存对齐问题

**位置**: `AIVideoProcessor.cpp:428`

```cpp
QImage tempImg(rgb24Buffer.data(), frameW, frameH, rgb24LineSize, QImage::Format_RGB888);
```

**潜在问题**：
- QImage 构造函数接受外部数据时，**不会复制数据**
- 如果 `rgb24Buffer.data()` 返回的指针不是 QImage 期望的对齐方式，可能导致问题

### 🟡 问题 4：Vulkan GPU 状态同步问题

**位置**: `AIEngine.cpp:310-331`

虽然模型同步已完成，但问题可能在于：
- GPU 命令队列中仍有未完成的操作
- Vulkan allocator 内部状态不一致

### 🟡 问题 5：线程安全问题

**位置**: `AIVideoProcessor.cpp:85-88`

```cpp
m_processingFuture = QtConcurrent::run([this, inputPath, outputPath]() {
    processInternal(inputPath, outputPath);
});
```

**分析**：
- `processInternal` 在工作线程执行
- `AIEngine` 的推理也在工作线程
- 可能存在跨线程资源访问问题

---

## 修复方案

### 修复 1：重构图像数据复制逻辑（高优先级）

**目标**：避免 QImage 引用外部数据，直接创建独立内存

**修改文件**: `src/core/video/AIVideoProcessor.cpp`

**修改内容**：

```cpp
// 替换第 414-455 行的代码

// 1. 使用对齐的缓冲区
const int frameW = decFrame->width;
const int frameH = decFrame->height;
const int rgb24LineSize = frameW * 3;

// 2. 直接创建独立内存的 QImage
QImage frameImg(frameW, frameH, QImage::Format_RGB888);
if (frameImg.isNull()) {
    qWarning() << "[AIVideoProcessor][DEBUG] Failed to allocate frameImg";
    av_frame_unref(decFrame);
    continue;
}

// 3. 设置目标参数
uint8_t* dst[4] = { frameImg.bits(), nullptr, nullptr, nullptr };
int dstStride[4] = { static_cast<int>(frameImg.bytesPerLine()), 0, 0, 0 };

// 4. 直接写入 QImage 内存
qInfo() << "[AIVideoProcessor][DEBUG] Frame" << frameCount << "calling sws_scale";
fflush(stdout);

int swsRet = sws_scale(decSwsCtx, decFrame->data, decFrame->linesize,
                       0, frameH, dst, dstStride);

qInfo() << "[AIVideoProcessor][DEBUG] Frame" << frameCount << "sws_scale done, ret:" << swsRet;
fflush(stdout);

if (swsRet != frameH) {
    qWarning() << "[AIVideoProcessor][DEBUG] sws_scale returned unexpected height:"
               << swsRet << "expected:" << frameH;
    av_frame_unref(decFrame);
    continue;
}

// 5. 验证图像有效性
if (frameImg.isNull() || frameImg.bits() == nullptr) {
    qWarning() << "[AIVideoProcessor][DEBUG] Frame" << frameCount << "frameImg is invalid";
    av_frame_unref(decFrame);
    continue;
}

qInfo() << "[AIVideoProcessor][DEBUG] Frame" << frameCount << "frameImg ready:"
        << frameImg.width() << "x" << frameImg.height()
        << "format:" << frameImg.format()
        << "bytesPerLine:" << frameImg.bytesPerLine();
fflush(stdout);
```

### 修复 2：增强 GPU 同步和内存屏障（高优先级）

**目标**：确保 GPU 操作完全完成后才进行 CPU 内存操作

**修改文件**: `src/core/AIEngine.cpp`

**修改内容**：

1. 在 `loadModel()` 末尾增加更强的同步：

```cpp
// 在 loadModel() 函数末尾，第 481 行后添加

// 【新增】添加内存屏障，确保 CPU 和 GPU 内存一致性
std::atomic_thread_fence(std::memory_order_seq_cst);

// 【新增】增加更长的等待时间，确保 GPU 完全空闲
QThread::msleep(150);  // 从 100ms 增加到 150ms

// 【新增】最终验证：尝试一次空操作确保 GPU 响应
{
    ncnn::VkCompute cmd(m_vkdev);
    cmd.submit_and_wait();
}

qInfo() << "[AIEngine][DEBUG] Vulkan sync after model load completed (enhanced)";
```

2. 在 `ensureVulkanReady()` 中增加内存屏障：

```cpp
// 在 ensureVulkanReady() 函数中添加

void AIEngine::ensureVulkanReady()
{
#if NCNN_VULKAN
    if (!m_gpuAvailable || !m_useGpu || !m_vkdev) {
        return;
    }

    QMutexLocker allocatorLock(&m_allocatorMutex);

    {
        ncnn::VkCompute cmd(m_vkdev);
        cmd.submit_and_wait();
    }

    // 【新增】添加内存屏障
    std::atomic_thread_fence(std::memory_order_seq_cst);

    if (m_blobVkAllocator) {
        m_blobVkAllocator->clear();
    }
    if (m_stagingVkAllocator) {
        m_stagingVkAllocator->clear();
    }

    qInfo() << "[AIEngine][DEBUG] ensureVulkanReady() completed (sync only)";
#endif
}
```

### 修复 3：增强 sws_scale 安全性（中优先级）

**目标**：验证 sws_scale 的输出，防止越界写入

**修改文件**: `src/core/video/AIVideoProcessor.cpp`

**修改内容**：

```cpp
// 在 sws_scale 调用前添加验证

// 验证 SwsContext 状态
if (!decSwsCtx) {
    qWarning() << "[AIVideoProcessor][DEBUG] SwsContext is null at frame" << frameCount;
    av_frame_unref(decFrame);
    continue;
}

// 验证源数据有效性
if (!decFrame->data[0]) {
    qWarning() << "[AIVideoProcessor][DEBUG] decFrame data is null at frame" << frameCount;
    av_frame_unref(decFrame);
    continue;
}

// 验证目标缓冲区大小
const size_t expectedBufferSize = static_cast<size_t>(rgb24LineSize * frameH);
if (rgb24Buffer.size() != expectedBufferSize) {
    qWarning() << "[AIVideoProcessor][DEBUG] Buffer size mismatch at frame" << frameCount
               << "expected:" << expectedBufferSize
               << "actual:" << rgb24Buffer.size();
    av_frame_unref(decFrame);
    continue;
}

// 记录 sws_scale 参数
qInfo() << "[AIVideoProcessor][DEBUG] sws_scale params:"
        << "srcW:" << decFrame->width
        << "srcH:" << decFrame->height
        << "dstW:" << frameW
        << "dstH:" << frameH
        << "srcStride:" << decFrame->linesize[0]
        << "dstStride:" << rgb24LineSize;
fflush(stdout);
```

### 修复 4：增加详细的调试日志（高优先级）

**目标**：在关键位置添加日志，帮助定位问题

**修改文件**: `src/core/video/AIVideoProcessor.cpp`

**修改内容**：

1. 在 `processFrame()` 函数中添加详细日志：

```cpp
QImage AIVideoProcessor::processFrame(const QImage& input)
{
    qInfo() << "[AIVideoProcessor][DEBUG] processFrame() called:"
            << "inputSize:" << input.width() << "x" << input.height()
            << "format:" << input.format()
            << "isNull:" << input.isNull()
            << "bits:" << (input.bits() != nullptr ? "valid" : "null")
            << "bytesPerLine:" << input.bytesPerLine();
    fflush(stdout);
    
    // ... 现有代码 ...
}
```

2. 在 AIEngine 推理函数中添加详细日志：

```cpp
QImage AIEngine::process(const QImage &input)
{
    // 推理互斥：同一时刻只允许一次推理
    QMutexLocker inferenceLocker(&m_inferenceMutex);
    
    // 【新增】详细调试信息
    qInfo() << "[AIEngine][DEBUG] process() called:"
            << "inputSize:" << input.width() << "x" << input.height()
            << "format:" << input.format()
            << "modelId:" << m_currentModelId
            << "modelLoaded:" << m_currentModel.isLoaded
            << "useGpu:" << m_useGpu
            << "gpuAvailable:" << m_gpuAvailable
            << "vulkanReady:" << m_vulkanReady.load()
            << "modelSyncComplete:" << m_modelSyncComplete.load();
    fflush(stdout);
    
    // ... 现有代码 ...
}
```

### 修复 5：智能图像尺寸适配（中优先级）

**目标**：处理各种比例大小的图像或视频，包括特殊比例

**修改文件**: `src/core/video/AIVideoProcessor.cpp`

**修改内容**：

```cpp
// 在 processInternal() 函数中添加尺寸验证和调整

// 验证输入尺寸
if (srcW <= 0 || srcH <= 0 || srcW > 16384 || srcH > 16384) {
    fail(tr("无效的视频尺寸: %1x%2").arg(srcW).arg(srcH));
    return;
}

// 智能调整输出尺寸（确保偶数尺寸）
const int scale = m_impl->modelInfo.scaleFactor;
const int outW = ((srcW * scale) & ~1);
const int outH = ((srcH * scale) & ~1);

// 验证输出尺寸
if (outW <= 0 || outH <= 0 || outW > 16384 || outH > 16384) {
    fail(tr("无效的输出尺寸: %1x%2").arg(outW).arg(outH));
    return;
}

qInfo() << "[AIVideoProcessor][DEBUG] Size validation passed:"
        << "srcSize:" << srcW << "x" << srcH
        << "outSize:" << outW << "x" << outH
        << "scale:" << scale;
```

---

## 实施步骤

### 阶段 1：紧急修复（立即实施）

1. **修复 1**：重构图像数据复制逻辑
   - 直接创建独立内存的 QImage
   - 避免 QImage 引用外部数据

2. **修复 2**：增强 GPU 同步和内存屏障
   - 在关键位置添加内存屏障
   - 增加 GPU 同步等待时间

3. **修复 4**：增加详细的调试日志
   - 在关键位置添加日志
   - 帮助定位问题

### 阶段 2：稳定性增强（后续实施）

1. **修复 3**：增强 sws_scale 安全性
   - 验证输入输出参数
   - 防止越界写入

2. **修复 5**：智能图像尺寸适配
   - 处理各种比例大小的图像
   - 添加尺寸验证

### 阶段 3：验证和测试

1. 构建项目
2. 运行应用程序
3. 测试各种尺寸的视频和图像
4. 检查日志文件
5. 验证修复效果

---

## 预期效果

1. **消除崩溃**：通过重构图像数据复制逻辑，避免堆损坏
2. **增强稳定性**：通过 GPU 同步和内存屏障，确保资源一致性
3. **提高可维护性**：通过详细日志，便于问题定位
4. **增强健壮性**：通过尺寸验证和智能调整，处理各种特殊情况

---

## 风险评估

1. **性能影响**：增加的同步和验证可能略微降低性能，但可接受
2. **兼容性**：修改不影响现有功能，仅增强稳定性
3. **测试范围**：需要测试各种尺寸和格式的视频/图像

---

## 文件修改清单

| 文件 | 修改内容 | 优先级 |
|------|----------|--------|
| `src/core/video/AIVideoProcessor.cpp` | 重构图像数据复制逻辑 | 高 |
| `src/core/AIEngine.cpp` | 增强 GPU 同步和内存屏障 | 高 |
| `src/core/video/AIVideoProcessor.cpp` | 增强调试日志 | 高 |
| `src/core/video/AIVideoProcessor.cpp` | 增强 sws_scale 安全性 | 中 |
| `src/core/video/AIVideoProcessor.cpp` | 智能图像尺寸适配 | 中 |
