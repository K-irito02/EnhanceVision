# 视频处理崩溃修复与架构优化计划

## 问题分析

### 1. 崩溃根本原因

通过代码分析，发现视频处理崩溃的主要原因：

#### 1.1 AIEngine::processVideoInternal 线程安全问题

```cpp
// AIEngine.cpp:1503
QImage resultImg = process(frameImg);  // 调用 process() 进行推理
// 恢复视频管线的处理状态：process() 会设 isProcessing=false 和 progress=1.0
m_isProcessing.store(true);
```

**问题**：
- `process()` 内部使用 `QMutexLocker inferenceLocker(&m_inferenceMutex)` 获取锁
- `process()` 完成时会发出信号（如 `processCompleted`），这些信号可能触发 UI 更新
- 视频处理在工作线程中运行，但信号发射可能导致跨线程问题
- `process()` 会重置 `m_isProcessing` 和 `progress`，视频管线需要手动恢复，容易出错

#### 1.2 GPU 状态损坏

```cpp
// AIEngine.cpp:1146-1150
if (m_gpuOomDetected.load()) {
    qWarning() << "[AIEngine][Tiled] GPU OOM detected, aborting to allow retry with smaller tiles";
    painter.end();
    return QImage();
}
```

视频处理连续调用 `process()` 时，如果某帧触发 GPU OOM，后续帧可能因 GPU 状态损坏而崩溃。

#### 1.3 FFmpeg 资源管理问题

```cpp
// AIEngine.cpp:1592-1601
AVFrame *rgbFrame = qimageToAvFrame(resultImg, decFrame->pts);
if (!rgbFrame) {
    qWarning() << "[AIEngine][Video] frame" << frameCount << "qimageToAvFrame failed";
    continue;  // 跳过但未释放 decFrame
}
```

#### 1.4 推理互斥锁问题

`process()` 使用 `m_inferenceMutex` 保护推理过程，但视频管线在循环中多次调用 `process()`，可能导致：
- 锁竞争严重
- 无法正确取消
- 状态不一致

### 2. 架构设计问题

#### 2.1 处理入口不统一

当前有两种处理路径：
- **AI推理模式**：`ProcessingEngine::startTask()` → `AIEngine::processAsync()` → `processVideoInternal()`
- **Shader模式**：`ProcessingEngine::startTask()` → `VideoProcessor::processVideoAsync()`

这导致：
- 代码重复
- 行为不一致
- 难以维护

#### 2.2 媒体类型识别分散

视频文件检测在 `AIEngine::processAsync()` 中：
```cpp
if (ImageUtils::isVideoFile(inputPath)) {
    processVideoInternal(inputPath, outputPath);
    return;
}
```

但 `ProcessingEngine::startTask()` 也有自己的判断逻辑，导致职责不清。

#### 2.3 参数自动调整不完善

`computeAutoParams()` 方法存在但：
- 未在视频处理前自动调用
- 未根据视频分辨率动态调整分块大小
- 未考虑视频帧率对处理策略的影响

## 解决方案

### 阶段一：修复视频处理崩溃（高优先级）

#### 1.1 重构 AIEngine::processVideoInternal

**目标**：消除 `process()` 与视频管线的状态冲突

**方案**：创建专用的帧处理方法 `processFrame()`，不修改全局状态

```cpp
// 新增方法：专用于视频帧处理，不修改全局状态
QImage AIEngine::processFrame(const QImage &input, const ModelInfo &model);
```

**实现要点**：
- 不获取 `m_inferenceMutex`（视频管线已有自己的互斥保护）
- 不修改 `m_isProcessing` 和 `m_progress`
- 使用传入的 `model` 快照，避免数据竞争
- 返回处理结果，不发出信号

#### 1.2 添加视频处理专用状态管理

```cpp
struct VideoProcessState {
    std::atomic<bool> isProcessing{false};
    std::atomic<double> progress{0.0};
    std::atomic<bool> cancelRequested{false};
    std::atomic<bool> gpuOomDetected{false};
    int64_t currentFrame{0};
    int64_t totalFrames{0};
};
```

#### 1.3 改进 GPU 错误恢复

- 每帧处理后检查 GPU 状态
- 检测到 OOM 时，自动降级到更小分块或 CPU 模式
- 添加 GPU 状态重置机制

#### 1.4 修复内存管理

- 确保 FFmpeg 资源正确释放
- 添加 RAII 包装器管理 AVFrame/AVPacket
- 检查所有可能的内存泄漏点

### 阶段二：统一媒体处理入口（中优先级）

#### 2.1 创建 MediaProcessor 统一接口

```cpp
class MediaProcessor : public QObject {
    Q_OBJECT
public:
    // 统一处理入口
    void processAsync(const QString &inputPath, const QString &outputPath,
                      const ProcessingParams &params);
    
    // 自动识别媒体类型
    MediaType detectMediaType(const QString &filePath);
    
    // 自动计算最优参数
    ProcessingParams computeOptimalParams(const QString &filePath,
                                          const QString &modelId);
};
```

#### 2.2 整合 AIEngine 和 VideoProcessor

- `AIEngine` 专注于单帧 AI 推理
- `VideoProcessor` 专注于视频编解码
- `MediaProcessor` 协调两者，提供统一入口

#### 2.3 自动媒体类型识别

```cpp
MediaType MediaProcessor::detectMediaType(const QString &filePath) {
    // 1. 检查文件扩展名
    // 2. 使用 FFmpeg 探测实际格式
    // 3. 返回准确的媒体类型
}
```

### 阶段三：自动参数调整（中优先级）

#### 3.1 视频处理前自动参数计算

```cpp
ProcessingParams MediaProcessor::computeOptimalParams(
    const QString &filePath, const QString &modelId) {
    
    // 1. 获取媒体信息（分辨率、帧率、时长）
    MediaInfo info = getMediaInfo(filePath);
    
    // 2. 根据模型类型和媒体信息计算参数
    ModelInfo model = m_modelRegistry->getModelInfo(modelId);
    
    // 3. 视频特殊处理
    if (info.isVideo) {
        // 降低分块大小以减少内存峰值
        // 考虑帧率调整处理策略
        // 预估处理时间
    }
    
    return params;
}
```

#### 3.2 动态参数调整

- 处理过程中根据实际性能动态调整
- GPU OOM 时自动降级
- 内存不足时启用分块

### 阶段四：错误处理与健壮性（中优先级）

#### 4.1 全局错误处理

```cpp
class ProcessingErrorHandler {
public:
    void handleError(ProcessingError error);
    bool shouldRetry(ProcessingError error);
    ProcessingStrategy getFallbackStrategy(ProcessingError error);
};
```

#### 4.2 资源监控

- 内存使用监控
- GPU 显存监控
- 处理进度监控

#### 4.3 优雅降级

- GPU 失败 → CPU 回退
- 大分块失败 → 小分块重试
- AI 推理失败 → 原帧保留

## 实施步骤

### 步骤 1：修复 AIEngine 视频处理崩溃（立即）

1. 创建 `processFrame()` 方法
2. 重构 `processVideoInternal()` 使用新方法
3. 添加视频专用状态管理
4. 改进 GPU 错误恢复
5. 修复内存管理问题

**文件修改**：
- `src/core/AIEngine.cpp`
- `include/EnhanceVision/core/AIEngine.h`

### 步骤 2：创建 MediaProcessor 统一入口（后续）

1. 创建 `MediaProcessor` 类
2. 实现媒体类型自动识别
3. 整合 AIEngine 和 VideoProcessor
4. 更新 ProcessingEngine 使用新接口

**新增文件**：
- `src/core/MediaProcessor.cpp`
- `include/EnhanceVision/core/MediaProcessor.h`

**修改文件**：
- `src/core/ProcessingEngine.cpp`

### 步骤 3：实现自动参数调整（后续）

1. 扩展 `computeOptimalParams()` 方法
2. 添加媒体信息获取功能
3. 实现动态参数调整
4. 添加性能监控

**修改文件**：
- `src/core/MediaProcessor.cpp`
- `src/core/AIEngine.cpp`

### 步骤 4：完善错误处理（后续）

1. 创建错误处理器
2. 添加资源监控
3. 实现优雅降级
4. 添加详细日志

**新增文件**：
- `src/core/ProcessingErrorHandler.cpp`
- `include/EnhanceVision/core/ProcessingErrorHandler.h`

## 测试计划

### 单元测试

1. `processFrame()` 方法测试
2. 媒体类型识别测试
3. 参数自动计算测试
4. 错误处理测试

### 集成测试

1. 视频处理完整流程测试
2. 图像处理完整流程测试
3. GPU/CPU 切换测试
4. 内存泄漏检测

### 压力测试

1. 大视频文件处理
2. 高分辨率视频处理
3. 长时间运行稳定性
4. 并发处理测试

## 风险评估

### 高风险

1. **GPU 状态损坏**：可能导致后续处理全部失败
   - 缓解：添加 GPU 状态检测和重置机制

2. **内存泄漏**：长时间视频处理可能耗尽内存
   - 缓解：添加内存监控和定期清理

### 中风险

1. **性能下降**：新的抽象层可能引入开销
   - 缓解：性能测试和优化

2. **兼容性问题**：修改可能影响现有功能
   - 缓解：充分的回归测试

## 预期成果

1. **崩溃问题修复**：视频处理不再崩溃
2. **统一处理入口**：图像和视频处理使用相同接口
3. **自动参数调整**：根据媒体特征自动优化参数
4. **健壮性提升**：完善的错误处理和降级机制
5. **用户体验改善**：更稳定、更智能的处理流程

## 时间估算

- 步骤 1（崩溃修复）：2-3 小时
- 步骤 2（统一入口）：3-4 小时
- 步骤 3（自动参数）：2-3 小时
- 步骤 4（错误处理）：2-3 小时
- 测试与验证：2-3 小时

**总计**：11-16 小时
