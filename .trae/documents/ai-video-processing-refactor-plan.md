# AI推理模式视频处理重构计划

## 一、任务概述

### 1.1 目标
1. 彻底移除AI推理模式中处理视频类型多媒体文件的所有相关代码及文件
2. 重新设计并实现AI推理模式的视频处理功能，满足以下核心要求：
   - 支持多种视频尺寸比例和主流视频格式
   - 实现完善的资源管理机制
   - 采用线程安全的设计架构
   - 实施内存泄漏防护措施
   - 优化视频处理算法和UI交互逻辑

### 1.2 当前架构分析

#### 现有视频处理代码位置
| 文件 | 代码位置 | 功能描述 |
|------|----------|----------|
| `src/core/AIEngine.cpp` | L1862-L2463 | `processVideoInternal()` - AI推理视频处理核心逻辑 |
| `src/core/AIEngine.cpp` | L1440-L1532 | `processFrame()` - 视频帧轻量级推理 |
| `src/core/AIEngine.cpp` | L1534-L1825 | `processTiledNoProgress()` / `processSingleNoProgress()` - 无进度版帧处理 |
| `src/core/AIEngine.cpp` | L1847-L1858 | `cleanupGpuMemory()` - GPU显存清理 |
| `include/EnhanceVision/core/AIEngine.h` | L247-L268 | 视频处理相关方法声明 |

#### 现有问题分析
1. **代码耦合度高**：视频处理逻辑直接嵌入AIEngine，与图像处理逻辑混合
2. **资源管理复杂**：FFmpeg资源分散管理，cleanup lambda容易遗漏
3. **线程安全问题**：视频处理中存在跨线程信号发射问题
4. **内存管理风险**：帧缓冲、SwsContext等资源管理不够健壮
5. **缺乏抽象层**：没有独立的视频处理抽象层

---

## 二、第一阶段：移除现有视频处理代码

### 2.1 需要移除的代码

#### AIEngine.h 移除项
```cpp
// 移除以下方法声明
void processVideoInternal(const QString &inputPath, const QString &outputPath);
QImage processFrame(const QImage &input, const ModelInfo &model, int tileSize);
QImage processTiledNoProgress(const QImage &input, const ModelInfo &model);
QImage processSingleNoProgress(const QImage &input, const ModelInfo &model);
void cleanupGpuMemory();
```

#### AIEngine.cpp 移除项
- `processVideoInternal()` 完整实现（约600行）
- `processFrame()` 完整实现（约100行）
- `processTiledNoProgress()` 完整实现（约300行）
- `processSingleNoProgress()` 完整实现（约20行）
- `cleanupGpuMemory()` 完整实现（约15行）
- FFmpeg头文件引用（extern "C"块）

#### ProcessingController.cpp 修改
- 移除对 `processVideoInternal` 的调用
- 保留 Shader 模式的 `VideoProcessor` 使用（该类用于Shader滤镜，不涉及AI推理）

### 2.2 移除步骤
1. 注释掉AIEngine.cpp中的FFmpeg头文件引用
2. 移除AIEngine.h中的视频处理方法声明
3. 移除AIEngine.cpp中的视频处理方法实现
4. 移除processAsync中对视频文件的检测和路由逻辑
5. 编译验证移除完整性

---

## 三、第二阶段：重新设计视频处理架构

### 3.1 架构设计原则

#### 设计模式应用
| 模式 | 应用场景 | 实现方式 |
|------|----------|----------|
| 工厂模式 | 创建视频处理器实例 | `VideoProcessorFactory` 根据视频格式创建对应处理器 |
| 观察者模式 | 进度通知和状态更新 | `IVideoProcessingObserver` 接口 |
| RAII模式 | 资源管理 | `FFmpegResource` 智能包装器 |
| 策略模式 | 编码器/解码器选择 | `IEncoderStrategy` / `IDecoderStrategy` |
| 线程池模式 | 并行帧处理 | `FrameProcessingThreadPool` |

### 3.2 新增文件结构

```
include/EnhanceVision/core/video/
├── VideoProcessorBase.h          # 视频处理器基类
├── AIVideoProcessor.h            # AI推理视频处理器
├── VideoProcessorFactory.h       # 视频处理器工厂
├── VideoResourceGuard.h          # 资源管理守护类
├── VideoProcessingTypes.h        # 视频处理类型定义
└── strategies/
    ├── IEncoderStrategy.h        # 编码器策略接口
    ├── H264EncoderStrategy.h     # H.264编码策略
    ├── H265EncoderStrategy.h     # H.265编码策略
    ├── IDecoderStrategy.h        # 解码器策略接口
    └── HardwareDecoderStrategy.h # 硬件解码策略

src/core/video/
├── VideoProcessorBase.cpp
├── AIVideoProcessor.cpp
├── VideoProcessorFactory.cpp
├── VideoResourceGuard.cpp
└── strategies/
    ├── H264EncoderStrategy.cpp
    ├── H265EncoderStrategy.cpp
    └── HardwareDecoderStrategy.cpp
```

### 3.3 核心类设计

#### 3.3.1 VideoProcessingTypes.h - 类型定义
```cpp
namespace EnhanceVision {

enum class VideoAspectRatio {
    Ratio_16_9,   // 横屏标准
    Ratio_4_3,    // 传统电视
    Ratio_1_1,    // 正方形
    Ratio_9_16,   // 竖屏
    Ratio_21_9,   // 超宽屏
    Ratio_Custom  // 自定义
};

enum class VideoFormat {
    MP4,
    AVI,
    MOV,
    MKV,
    WebM,
    FLV,
    Unknown
};

struct VideoMetadata {
    int width = 0;
    int height = 0;
    VideoAspectRatio aspectRatio = VideoAspectRatio::Ratio_16_9;
    VideoFormat format = VideoFormat::Unknown;
    double frameRate = 0.0;
    int64_t durationMs = 0;
    int64_t totalFrames = 0;
    bool hasAudio = false;
    QString codecName;
};

struct VideoProcessingConfig {
    int tileSize = 0;
    double outscale = 1.0;
    bool useGpu = true;
    int maxConcurrentFrames = 2;
    int gpuCleanupInterval = 10;
    QString outputCodec = "h264";
    int quality = 18;
};

} // namespace EnhanceVision
```

#### 3.3.2 VideoResourceGuard.h - RAII资源管理
```cpp
namespace EnhanceVision {

class VideoResourceGuard {
public:
    VideoResourceGuard();
    ~VideoResourceGuard();
    
    // 禁止拷贝
    VideoResourceGuard(const VideoResourceGuard&) = delete;
    VideoResourceGuard& operator=(const VideoResourceGuard&) = delete;
    
    // 允许移动
    VideoResourceGuard(VideoResourceGuard&& other) noexcept;
    VideoResourceGuard& operator=(VideoResourceGuard&& other) noexcept;
    
    // 资源获取
    bool initializeInput(const QString& path);
    bool initializeOutput(const QString& path, const VideoMetadata& meta);
    
    // 资源访问
    AVFormatContext* inputFormatContext() const;
    AVFormatContext* outputFormatContext() const;
    AVCodecContext* decoderContext() const;
    AVCodecContext* encoderContext() const;
    SwsContext* decoderSwsContext() const;
    SwsContext* encoderSwsContext() const;
    
    // 资源有效性检查
    bool isValid() const;
    QString lastError() const;
    
private:
    struct Impl;
    std::unique_ptr<Impl> m_impl;
};

} // namespace EnhanceVision
```

#### 3.3.3 AIVideoProcessor.h - AI推理视频处理器
```cpp
namespace EnhanceVision {

class AIEngine;

class AIVideoProcessor : public QObject {
    Q_OBJECT
    
public:
    explicit AIVideoProcessor(QObject* parent = nullptr);
    ~AIVideoProcessor() override;
    
    void setAIEngine(AIEngine* engine);
    void setModelInfo(const ModelInfo& model);
    void setConfig(const VideoProcessingConfig& config);
    
    void processAsync(const QString& inputPath, const QString& outputPath);
    void cancel();
    
    bool isProcessing() const;
    double progress() const;
    
signals:
    void progressChanged(double progress);
    void processingChanged(bool processing);
    void completed(bool success, const QString& resultPath, const QString& error);
    void warning(const QString& message);
    
private:
    void processFrameBatch(const QList<QImage>& frames, QList<QImage>& results);
    void updateProgress(double progress);
    void cleanup();
    
    struct Impl;
    std::unique_ptr<Impl> m_impl;
};

} // namespace EnhanceVision
```

### 3.4 线程安全设计

#### 3.4.1 线程模型
```
┌─────────────────────────────────────────────────────────────┐
│                      UI Thread                               │
│  ┌─────────────┐    ┌─────────────┐    ┌─────────────┐     │
│  │ QML/Qt Quick│ ←→ │ProcessingCtrl│ ←→ │ MessageModel│     │
│  └─────────────┘    └──────┬──────┘    └─────────────┘     │
└─────────────────────────────┼───────────────────────────────┘
                              │ Qt::QueuedConnection
                              ▼
┌─────────────────────────────────────────────────────────────┐
│                    Worker Thread                             │
│  ┌─────────────────────────────────────────────────────┐   │
│  │              AIVideoProcessor                        │   │
│  │  ┌──────────┐  ┌──────────┐  ┌──────────┐          │   │
│  │  │ Decoder  │→ │FrameQueue│→ │ Encoder  │          │   │
│  │  └──────────┘  └────┬─────┘  └──────────┘          │   │
│  │                     │                                │   │
│  │                     ▼                                │   │
│  │           ┌─────────────────┐                       │   │
│  │           │  AI Inference   │                       │   │
│  │           │  (AIEnginePool) │                       │   │
│  │           └─────────────────┘                       │   │
│  └─────────────────────────────────────────────────────┘   │
└─────────────────────────────────────────────────────────────┘
```

#### 3.4.2 同步机制
1. **互斥锁保护**：使用 `QMutex` 保护共享状态
2. **原子变量**：使用 `std::atomic` 管理取消标志和进度
3. **信号槽队列**：跨线程通信使用 `Qt::QueuedConnection`
4. **线程安全队列**：帧队列使用线程安全实现

### 3.5 内存管理设计

#### 3.5.1 内存分配策略
```cpp
class FrameBufferPool {
public:
    FrameBufferPool(size_t poolSize, const QSize& frameSize);
    ~FrameBufferPool();
    
    std::shared_ptr<QImage> acquire();
    void release(std::shared_ptr<QImage> buffer);
    
    size_t totalAllocated() const;
    size_t inUse() const;
    
private:
    QMutex m_mutex;
    QList<std::shared_ptr<QImage>> m_available;
    QSet<std::shared_ptr<QImage>> m_inUse;
    size_t m_poolSize;
    QSize m_frameSize;
};
```

#### 3.5.2 内存泄漏防护
1. **智能指针**：所有动态分配使用 `std::unique_ptr` / `std::shared_ptr`
2. **RAII包装器**：FFmpeg资源使用 `VideoResourceGuard`
3. **对象池**：帧缓冲使用对象池复用
4. **定期检测**：集成内存检测钩子

### 3.6 支持的视频格式和比例

#### 3.6.1 支持的视频格式
| 格式 | 扩展名 | 编码器 | 备注 |
|------|--------|--------|------|
| MP4 | .mp4 | H.264/H.265 | 主要输出格式 |
| AVI | .avi | MPEG-4 | 兼容旧设备 |
| MOV | .mov | H.264/ProRes | Apple生态 |
| MKV | .mkv | H.264/H.265/VP9 | 开源容器 |
| WebM | .webm | VP9/AV1 | Web优化 |

#### 3.6.2 支持的视频比例
| 比例 | 分辨率示例 | 应用场景 |
|------|------------|----------|
| 16:9 | 1920×1080, 3840×2160 | 标准横屏 |
| 4:3 | 1440×1080, 640×480 | 传统电视 |
| 1:1 | 1080×1080 | 社交媒体 |
| 9:16 | 1080×1920 | 竖屏视频 |
| 21:9 | 2560×1080 | 超宽屏电影 |

### 3.7 UI响应性优化

#### 3.7.1 进度更新策略
```cpp
// 进度更新节流
constexpr double kProgressEmitDelta = 0.01;      // 最小进度变化
constexpr qint64 kProgressEmitIntervalMs = 100;  // 最小更新间隔

void AIVideoProcessor::updateProgress(double progress) {
    const qint64 now = QDateTime::currentMSecsSinceEpoch();
    const double current = m_progress.load();
    
    if (std::abs(progress - current) >= kProgressEmitDelta ||
        (now - m_lastProgressEmitMs) >= kProgressEmitIntervalMs ||
        progress >= 1.0) {
        m_progress.store(progress);
        m_lastProgressEmitMs = now;
        emit progressChanged(progress);
    }
}
```

#### 3.7.2 帧处理批量化
```cpp
// 批量处理减少GPU上下文切换
constexpr int kFrameBatchSize = 3;

void AIVideoProcessor::processFrameBatch(
    const QList<QImage>& frames, 
    QList<QImage>& results) 
{
    results.reserve(frames.size());
    for (const auto& frame : frames) {
        QImage result = m_aiEngine->processFrame(frame, m_model, m_config.tileSize);
        results.append(std::move(result));
    }
}
```

---

## 四、第三阶段：集成与测试

### 4.1 ProcessingController 集成

#### 4.1.1 修改点
1. 添加 `AIVideoProcessor` 成员管理
2. 修改 `startTask()` 中的视频处理逻辑
3. 添加视频处理进度回调
4. 添加视频处理取消逻辑

#### 4.1.2 集成代码示例
```cpp
void ProcessingController::startAIVideoTask(
    const QString& taskId,
    const QString& inputPath,
    const QString& outputPath,
    const Message& message) 
{
    auto processor = std::make_unique<AIVideoProcessor>();
    processor->setAIEngine(m_aiEngine);
    processor->setModelInfo(m_modelRegistry->getModelInfo(message.aiParams.modelId));
    
    VideoProcessingConfig config;
    config.tileSize = message.aiParams.tileSize;
    config.useGpu = message.aiParams.useGpu;
    processor->setConfig(config);
    
    connect(processor.get(), &AIVideoProcessor::progressChanged,
            this, [this, taskId](double progress) {
        updateTaskProgress(taskId, static_cast<int>(progress * 100));
    });
    
    connect(processor.get(), &AIVideoProcessor::completed,
            this, [this, taskId](bool success, const QString& result, const QString& error) {
        if (success) {
            completeTask(taskId, result);
        } else {
            failTask(taskId, error);
        }
    });
    
    m_activeVideoProcessors[taskId] = std::move(processor);
    m_activeVideoProcessors[taskId]->processAsync(inputPath, outputPath);
}
```

### 4.2 测试计划

#### 4.2.1 单元测试
| 测试项 | 测试内容 |
|--------|----------|
| VideoResourceGuard | 资源获取/释放正确性 |
| FrameBufferPool | 内存池分配/回收 |
| AIVideoProcessor | 处理流程正确性 |
| 进度更新 | 节流逻辑正确性 |

#### 4.2.2 集成测试
| 测试项 | 测试内容 |
|--------|----------|
| 视频格式兼容 | MP4/AVI/MOV/MKV 输入输出 |
| 视频比例处理 | 16:9/4:3/1:1/9:16 正确处理 |
| 取消操作 | 取消后资源正确释放 |
| 内存监控 | 长时间运行无内存泄漏 |

#### 4.2.3 性能测试
| 测试项 | 指标 |
|--------|------|
| 帧处理速度 | FPS ≥ 5 (1080p) |
| 内存占用 | 峰值 < 2GB |
| UI响应 | 操作延迟 < 100ms |

---

## 五、实施步骤

### 5.1 第一阶段：移除代码（预计1天）
1. [ ] 移除 AIEngine.h 中的视频处理方法声明
2. [ ] 移除 AIEngine.cpp 中的视频处理方法实现
3. [ ] 移除 FFmpeg 头文件引用
4. [ ] 修改 processAsync 移除视频检测逻辑
5. [ ] 编译验证移除完整性
6. [ ] 运行测试确保图像处理正常

### 5.2 第二阶段：实现新架构（预计3天）
1. [ ] 创建 video 子目录结构
2. [ ] 实现 VideoProcessingTypes.h
3. [ ] 实现 VideoResourceGuard
4. [ ] 实现 AIVideoProcessor
5. [ ] 实现编码器策略类
6. [ ] 实现解码器策略类
7. [ ] 实现 VideoProcessorFactory
8. [ ] 编写单元测试

### 5.3 第三阶段：集成测试（预计2天）
1. [ ] 修改 ProcessingController 集成新处理器
2. [ ] 修改 AIEngine 添加 processFrame 接口
3. [ ] 运行集成测试
4. [ ] 运行性能测试
5. [ ] 内存泄漏检测
6. [ ] UI响应性测试

### 5.4 第四阶段：文档更新（预计0.5天）
1. [ ] 更新架构文档
2. [ ] 更新 API 文档
3. [ ] 更新开发笔记

---

## 六、风险评估与缓解

### 6.1 技术风险
| 风险 | 影响 | 缓解措施 |
|------|------|----------|
| FFmpeg API 变化 | 中 | 封装抽象层隔离 |
| GPU 驱动兼容性 | 高 | 提供CPU回退方案 |
| 内存碎片化 | 中 | 使用对象池复用 |
| 线程死锁 | 高 | 严格锁顺序，超时机制 |

### 6.2 进度风险
| 风险 | 影响 | 缓解措施 |
|------|------|----------|
| 设计复杂度超预期 | 中 | 分阶段实施，优先核心功能 |
| 测试发现重大问题 | 高 | 预留缓冲时间 |
| 第三方库问题 | 低 | 使用稳定版本 |

---

## 七、验收标准

### 7.1 功能验收
- [ ] 支持 MP4/AVI/MOV/MKV 格式输入输出
- [ ] 支持 16:9/4:3/1:1/9:16 视频比例
- [ ] 视频处理进度正确显示
- [ ] 取消操作正确响应
- [ ] 错误信息正确反馈

### 7.2 质量验收
- [ ] 无内存泄漏（Valgrind/ASan 检测通过）
- [ ] 无线程安全问题（ThreadSanitizer 检测通过）
- [ ] 代码覆盖率 ≥ 70%
- [ ] 静态分析无警告

### 7.3 性能验收
- [ ] 1080p 视频处理 FPS ≥ 5
- [ ] 内存峰值 < 2GB
- [ ] UI 操作响应 < 100ms
- [ ] 长时间运行稳定（2小时测试）

---

## 八、附录

### A. 相关文件清单

#### 需要修改的文件
1. `include/EnhanceVision/core/AIEngine.h`
2. `src/core/AIEngine.cpp`
3. `include/EnhanceVision/controllers/ProcessingController.h`
4. `src/controllers/ProcessingController.cpp`

#### 需要新增的文件
1. `include/EnhanceVision/core/video/VideoProcessingTypes.h`
2. `include/EnhanceVision/core/video/VideoResourceGuard.h`
3. `include/EnhanceVision/core/video/VideoProcessorBase.h`
4. `include/EnhanceVision/core/video/AIVideoProcessor.h`
5. `include/EnhanceVision/core/video/VideoProcessorFactory.h`
6. `src/core/video/VideoResourceGuard.cpp`
7. `src/core/video/VideoProcessorBase.cpp`
8. `src/core/video/AIVideoProcessor.cpp`
9. `src/core/video/VideoProcessorFactory.cpp`

### B. 参考资料
1. FFmpeg Documentation: https://ffmpeg.org/documentation.html
2. NCNN Documentation: https://github.com/Tencent/ncnn
3. Qt Thread Basics: https://doc.qt.io/qt-6/thread-basics.html
4. RAII Pattern: https://en.cppreference.com/w/cpp/raii
