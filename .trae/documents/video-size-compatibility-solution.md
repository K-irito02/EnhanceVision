# AI推理模式下视频尺寸兼容性解决方案

## 1. 问题分析

### 1.1 当前架构现状

通过代码分析，发现以下关键问题：

| 组件 | 现状 | 问题 |
|------|------|------|
| **AIVideoProcessor** | 直接使用原始视频尺寸处理 | 无尺寸兼容性检测 |
| **AIEngine** | 支持分块处理大图，有自动分块计算 | 仅针对图像优化，视频场景未适配 |
| **VideoResourceGuard** | RAII资源管理，FFmpeg封装 | 无尺寸适配接口 |
| **VideoProcessingConfig** | 有tileSize/outscale配置 | 无尺寸兼容性策略配置 |

### 1.2 核心问题

1. **尺寸不兼容导致推理失败**：某些AI模型对输入尺寸有限制（如必须为特定倍数）
2. **超大视频导致OOM**：高分辨率视频（4K/8K）直接处理可能耗尽显存
3. **宽高比异常**：超宽/超窄视频可能导致AI模型输出失真
4. **视频格式不兼容**：某些视频编码格式、像素格式可能不被支持或需要特殊处理
5. **无预警机制**：用户无法提前知道视频是否兼容

### 1.3 视频格式兼容性分析

通过代码分析，当前项目存在以下格式兼容性问题：

| 问题类型 | 当前状态 | 风险等级 |
|----------|----------|----------|
| **容器格式** | 仅支持MP4输出，输入格式依赖FFmpeg | 中 |
| **视频编码** | 优先NVENC，回退libx264，不支持HEVC/VP9输出 | 中 |
| **像素格式** | 解码后转RGB24，编码用YUV420P，其他格式可能有问题 | 高 |
| **音频编码** | 直接复制音频流，可能不兼容MP4容器 | 高 |
| **色彩空间** | 未处理BT.709/BT.2020等色彩空间转换 | 中 |

**具体问题场景**：

1. **10-bit视频**：解码后精度丢失，可能导致色彩断层
2. **HDR视频**：未处理HDR到SDR的色调映射，输出过暗或过亮
3. **非标准帧率**：可能导致音画不同步
4. **多音轨视频**：仅复制第一个音轨，可能丢失其他音轨
5. **Alpha通道视频**：透明通道信息丢失

### 1.4 设计目标

- **自动检测**：处理前自动检测视频尺寸、格式与AI模型兼容性
- **智能适配**：对不兼容视频自动调整到最优尺寸和格式
- **格式转换**：正确处理各种像素格式、色彩空间、HDR视频
- **质量保证**：调整后保持内容完整性，推理质量满足要求
- **用户友好**：提供清晰的进度和状态反馈
- **性能优化**：避免不必要的尺寸转换，减少性能损耗

---

## 2. 架构设计

### 2.1 模块架构

```
┌─────────────────────────────────────────────────────────────────┐
│                        AIVideoProcessor                          │
│  (现有视频处理流程扩展)                                           │
└───────────────────────────┬─────────────────────────────────────┘
                            │
                            ▼
┌─────────────────────────────────────────────────────────────────┐
│                 VideoCompatibilityAnalyzer (新增)                │
│  ┌──────────────┐  ┌──────────────┐  ┌──────────────┐          │
│  │尺寸检测器     │  │格式检测器    │  │兼容性分析器  │          │
│  │SizeDetector  │  │FormatDetector│  │Analyzer      │          │
│  └──────────────┘  └──────────────┘  └──────────────┘          │
└───────────────────────────┬─────────────────────────────────────┘
                            │
                            ▼
┌─────────────────────────────────────────────────────────────────┐
│                    VideoSizeAdapter (新增)                       │
│  ┌──────────────┐  ┌──────────────┐  ┌──────────────┐          │
│  │尺寸调整器     │  │格式转换器    │  │色彩空间处理  │          │
│  │SizeResizer   │  │FormatConverter│ │ColorSpace    │          │
│  └──────────────┘  └──────────────┘  └──────────────┘          │
└───────────────────────────┬─────────────────────────────────────┘
                            │
                            ▼
┌─────────────────────────────────────────────────────────────────┐
│                    VideoFrameBuffer (新增)                       │
│  帧缓冲池 + 格式转换 + 线程安全队列                               │
└───────────────────────────┬─────────────────────────────────────┘
                            │
                            ▼
┌─────────────────────────────────────────────────────────────────┐
│                        AIEngine (现有)                           │
│  NCNN推理 + 分块处理 + GPU OOM自动降级                           │
└─────────────────────────────────────────────────────────────────┘
```

### 2.2 数据流设计

```
输入视频 → 尺寸/格式检测 → 兼容性分析 → 策略决策
                                        │
                    ┌───────────────────┼───────────────────┐
                    ▼                   ▼                   ▼
              直接处理             尺寸/格式调整        分块处理
                    │                   │                   │
                    └───────────────────┼───────────────────┘
                                        ▼
                                AI推理处理
                                        │
                                        ▼
                                输出视频
```

---

## 3. 核心组件设计

### 3.1 VideoCompatibilityAnalyzer（视频兼容性分析器）

```cpp
// include/EnhanceVision/core/video/VideoCompatibilityAnalyzer.h

namespace EnhanceVision {

enum class SizeCompatibility {
    FullyCompatible,      // 完全兼容，无需调整
    NeedsPadding,         // 需要填充到目标倍数
    NeedsDownscale,       // 需要缩小（超出安全范围）
    NeedsTiling,          // 需要分块处理（超大尺寸）
    Incompatible          // 完全不兼容（需用户确认）
};

enum class FormatCompatibility {
    FullySupported,       // 完全支持
    NeedsTranscode,       // 需要转码
    NeedsColorConvert,    // 需要色彩空间转换
    NeedsToneMapping,     // 需要HDR色调映射
    PartiallySupported,   // 部分支持（可能丢失信息）
    NotSupported          // 不支持
};

struct VideoCompatibilityReport {
    // 尺寸兼容性
    SizeCompatibility sizeCompatibility;
    QSize originalSize;
    QSize adaptedSize;
    QSize outputSize;
    int recommendedTileSize;
    QString sizeAdaptationReason;
    
    // 格式兼容性
    FormatCompatibility formatCompatibility;
    QString containerFormat;
    QString videoCodec;
    QString pixelFormat;
    bool isHDR;
    bool hasAlpha;
    int bitDepth;
    double frameRate;
    QString formatAdaptationReason;
    
    // 综合评估
    bool requiresUserConfirmation;
    bool canProcess;
    double estimatedMemoryMB;
    double estimatedGpuMemoryMB;
    qint64 estimatedProcessingTimeMs;
    QStringList warnings;
    QStringList errors;
};

struct SizeConstraints {
    int minWidth = 64;
    int maxWidth = 8192;
    int minHeight = 64;
    int maxHeight = 8192;
    int alignmentMultiple = 1;
    qint64 maxPixelsForSinglePass = 2048LL * 2048;
    double maxAspectRatio = 4.0;
    double minAspectRatio = 0.25;
};

struct FormatConstraints {
    QStringList supportedContainers = {"mp4", "mov", "mkv", "avi", "webm"};
    QStringList supportedVideoCodecs = {"h264", "hevc", "vp9", "av1"};
    QStringList supportedPixelFormats = {"yuv420p", "yuv422p", "yuv444p", "rgb24"};
    int maxBitDepth = 8;
    bool supportsHDR = false;
    bool supportsAlpha = false;
};

class VideoCompatibilityAnalyzer : public QObject {
    Q_OBJECT
    
public:
    explicit VideoCompatibilityAnalyzer(QObject* parent = nullptr);
    ~VideoCompatibilityAnalyzer() override;
    
    void setModelInfo(const ModelInfo& model);
    void setSizeConstraints(const SizeConstraints& constraints);
    void setFormatConstraints(const FormatConstraints& constraints);
    
    VideoCompatibilityReport analyze(const QString& videoPath) const;
    VideoCompatibilityReport analyze(const VideoMetadata& metadata) const;
    
    static SizeConstraints getDefaultSizeConstraints();
    static FormatConstraints getDefaultFormatConstraints();
    static SizeConstraints getSizeConstraintsForModel(const ModelInfo& model);
    
signals:
    void analysisCompleted(const VideoCompatibilityReport& report);
    void incompatibleDetected(const QString& reason);
    
private:
    SizeCompatibility analyzeSize(const QSize& size, const ModelInfo& model) const;
    FormatCompatibility analyzeFormat(const VideoMetadata& metadata) const;
    double estimateMemoryUsage(const QSize& size, const ModelInfo& model) const;
    double estimateGpuMemoryUsage(const QSize& size, const ModelInfo& model) const;
    
    struct Impl;
    std::unique_ptr<Impl> m_impl;
};

} // namespace EnhanceVision
```

### 3.2 VideoSizeAdapter（视频尺寸适配器）

```cpp
// include/EnhanceVision/core/video/VideoSizeAdapter.h

namespace EnhanceVision {

class VideoSizeAdapter : public QObject {
    Q_OBJECT
    
public:
    explicit VideoSizeAdapter(QObject* parent = nullptr);
    ~VideoSizeAdapter() override;
    
    void setModelInfo(const ModelInfo& model);
    void setConstraints(const SizeConstraints& constraints);
    
    SizeAdaptationResult analyze(const QSize& videoSize) const;
    QImage adaptFrame(const QImage& frame, const SizeAdaptationResult& adaptation) const;
    QImage restoreFrame(const QImage& processed, const SizeAdaptationResult& adaptation) const;
    
    static SizeConstraints getDefaultConstraints();
    static SizeConstraints getConstraintsForModel(const ModelInfo& model);
    
signals:
    void adaptationRequired(const QString& reason, const QSize& original, const QSize& adapted);
    
private:
    struct Impl;
    std::unique_ptr<Impl> m_impl;
};

} // namespace EnhanceVision
```

### 3.3 VideoFormatConverter（视频格式转换器）

```cpp
// include/EnhanceVision/core/video/VideoFormatConverter.h

namespace EnhanceVision {

struct ColorSpaceInfo {
    QString primaries;      // bt709, bt2020
    QString transfer;       // bt709, smpte2084, hlg
    QString matrix;         // bt709, bt2020nc
    bool isHDR;
    bool isFullRange;
};

class VideoFormatConverter : public QObject {
    Q_OBJECT
    
public:
    explicit VideoFormatConverter(QObject* parent = nullptr);
    ~VideoFormatConverter() override;
    
    QImage convertFrame(const QImage& frame, 
                        const ColorSpaceInfo& srcColorSpace,
                        const ColorSpaceInfo& dstColorSpace) const;
    
    QImage applyToneMapping(const QImage& hdrFrame, 
                            double peakLuminance = 1000.0) const;
    
    QImage convertPixelFormat(const QImage& frame, 
                              AVPixelFormat srcFmt,
                              AVPixelFormat dstFmt) const;
    
    QImage handleAlphaChannel(const QImage& frame, 
                              bool preserveAlpha = false) const;
    
    static ColorSpaceInfo detectColorSpace(AVFrame* avFrame);
    static bool needsToneMapping(const ColorSpaceInfo& info);
    
private:
    struct Impl;
    std::unique_ptr<Impl> m_impl;
};

} // namespace EnhanceVision
```

### 3.4 VideoFrameBuffer（视频帧缓冲池）

```cpp
// include/EnhanceVision/core/video/VideoFrameBuffer.h

namespace EnhanceVision {

struct FrameBufferSlot {
    QImage frame;
    int64_t pts;
    int frameIndex;
    bool isProcessed;
};

class VideoFrameBuffer : public QObject {
    Q_OBJECT
    
public:
    explicit VideoFrameBuffer(int maxSlots = 4, QObject* parent = nullptr);
    ~VideoFrameBuffer() override;
    
    bool pushFrame(const QImage& frame, int64_t pts, int frameIndex);
    bool popFrame(FrameBufferSlot& slot);
    
    void setMaxSlots(int maxSlots);
    int maxSlots() const;
    int usedSlots() const;
    
    void clear();
    bool isEmpty() const;
    bool isFull() const;
    
signals:
    void frameAvailable();
    void bufferCleared();
    
private:
    QQueue<FrameBufferSlot> m_buffer;
    QMutex m_mutex;
    QWaitCondition m_notFull;
    QWaitCondition m_notEmpty;
    int m_maxSlots;
};

} // namespace EnhanceVision
```

### 3.3 扩展 VideoProcessingConfig

```cpp
// 扩展 VideoProcessingTypes.h 中的 VideoProcessingConfig

struct VideoProcessingConfig {
    // 现有字段
    int tileSize = 0;
    double outscale = 1.0;
    bool useGpu = true;
    int maxConcurrentFrames = 2;
    int gpuCleanupInterval = 10;
    QString outputCodec = "h264";
    int quality = 18;
    int outputWidth = 0;
    int outputHeight = 0;
    
    // 新增字段：尺寸适配策略
    enum class SizeAdaptationPolicy {
        Auto,           // 自动决策（推荐）
        AlwaysAdapt,    // 总是调整到最优尺寸
        NeverAdapt,     // 从不调整（可能失败）
        UserConfirm     // 需要用户确认
    };
    
    SizeAdaptationPolicy sizeAdaptationPolicy = SizeAdaptationPolicy::Auto;
    bool preserveAspectRatio = true;        // 保持宽高比
    int maxOutputWidth = 4096;              // 最大输出宽度
    int maxOutputHeight = 4096;             // 最大输出高度
    bool enableSmartTiling = true;          // 启用智能分块
    int safetyMarginPercent = 10;           // 安全边际百分比
};
```

---

## 4. 核心算法设计

### 4.1 尺寸兼容性分析算法

```cpp
SizeAdaptationResult VideoSizeAdapter::analyze(const QSize& videoSize) const
{
    SizeAdaptationResult result;
    result.originalSize = videoSize;
    result.adaptedSize = videoSize;
    
    const int w = videoSize.width();
    const int h = videoSize.height();
    const qint64 pixels = static_cast<qint64>(w) * h;
    const double aspectRatio = static_cast<double>(w) / h;
    
    const auto& constraints = m_impl->constraints;
    const auto& model = m_impl->modelInfo;
    
    // 1. 检查尺寸边界
    if (w < constraints.minWidth || h < constraints.minHeight) {
        result.compatibility = SizeCompatibility::Incompatible;
        result.adaptationReason = tr("视频尺寸过小，低于模型最低要求");
        result.requiresUserConfirmation = true;
        return result;
    }
    
    // 2. 检查宽高比
    if (aspectRatio > constraints.maxAspectRatio || 
        aspectRatio < constraints.minAspectRatio) {
        result.compatibility = SizeCompatibility::NeedsPadding;
        result.adaptationReason = tr("视频宽高比异常，需要填充处理");
        // 计算填充后的尺寸
        if (aspectRatio > constraints.maxAspectRatio) {
            int targetH = static_cast<int>(w / constraints.maxAspectRatio);
            result.adaptedSize = QSize(w, targetH);
        } else {
            int targetW = static_cast<int>(h * constraints.maxAspectRatio);
            result.adaptedSize = QSize(targetW, h);
        }
    }
    
    // 3. 检查对齐要求
    if (constraints.alignmentMultiple > 1) {
        int alignedW = ((w + constraints.alignmentMultiple - 1) 
                        / constraints.alignmentMultiple) 
                       * constraints.alignmentMultiple;
        int alignedH = ((h + constraints.alignmentMultiple - 1) 
                        / constraints.alignmentMultiple) 
                       * constraints.alignmentMultiple;
        if (alignedW != w || alignedH != h) {
            result.compatibility = SizeCompatibility::NeedsPadding;
            result.adaptedSize = QSize(alignedW, alignedH);
            result.adaptationReason = tr("视频尺寸需要填充以匹配模型要求");
        }
    }
    
    // 4. 检查是否超出单次推理限制
    if (pixels > constraints.maxPixelsForSinglePass) {
        if (model.tileSize > 0) {
            result.compatibility = SizeCompatibility::NeedsTiling;
            result.recommendedTileSize = computeOptimalTileSize(videoSize, model);
            result.adaptationReason = tr("视频尺寸较大，将使用分块处理");
        } else {
            result.compatibility = SizeCompatibility::NeedsDownscale;
            double scale = std::sqrt(
                static_cast<double>(constraints.maxPixelsForSinglePass) / pixels);
            result.adaptedSize = QSize(
                static_cast<int>(w * scale),
                static_cast<int>(h * scale));
            result.adaptationReason = tr("视频尺寸超出安全范围，需要缩小处理");
        }
    }
    
    // 5. 估算内存需求
    result.estimatedMemoryMB = estimateMemoryUsage(result.adaptedSize, model);
    result.estimatedGpuMemoryMB = estimateGpuMemoryUsage(result.adaptedSize, model);
    
    // 6. 计算输出尺寸
    result.outputSize = QSize(
        result.adaptedSize.width() * model.scaleFactor,
        result.adaptedSize.height() * model.scaleFactor);
    
    if (result.compatibility == SizeCompatibility::FullyCompatible) {
        result.adaptationReason = tr("视频尺寸完全兼容，无需调整");
    }
    
    return result;
}
```

### 4.2 智能分块大小计算算法

```cpp
int VideoSizeAdapter::computeOptimalTileSize(const QSize& videoSize, 
                                              const ModelInfo& model) const
{
    const int w = videoSize.width();
    const int h = videoSize.height();
    const qint64 pixels = static_cast<qint64>(w) * h;
    
    // 基于模型层数动态调整kFactor
    double kFactor = 8.0;
    const int layerCount = model.layerCount;
    
    if (layerCount > 500) {
        kFactor = 48.0;
    } else if (layerCount > 200) {
        kFactor = 32.0;
    } else if (layerCount > 50) {
        kFactor = 16.0;
    } else if (layerCount > 0) {
        kFactor = 10.0;
    }
    
    // 估算可用显存
    qint64 availableMB = 1024;  // 保守估计1GB
    if (m_impl->gpuOomDetected) {
        availableMB = 256;  // OOM后更保守
    }
    
    // 计算最大安全分块大小
    const int channels = model.outputChannels > 0 ? model.outputChannels : 3;
    const int scale = std::max(1, model.scaleFactor);
    
    auto memForTile = [&](int tile) -> double {
        const qint64 px = static_cast<qint64>(tile) * tile;
        const qint64 bytes = px * channels * sizeof(float) * scale * scale;
        return static_cast<double>(bytes) * kFactor / (1024 * 1024);
    };
    
    const int minTile = 64;
    const int maxTile = std::min(512, std::max(w, h));
    const int step = 32;
    
    int bestTile = minTile;
    for (int t = maxTile; t >= minTile; t -= step) {
        if (memForTile(t) <= static_cast<double>(availableMB)) {
            bestTile = t;
            break;
        }
    }
    
    return bestTile;
}
```

### 4.3 格式兼容性分析算法

```cpp
FormatCompatibility VideoCompatibilityAnalyzer::analyzeFormat(
    const VideoMetadata& metadata) const
{
    const auto& constraints = m_impl->formatConstraints;
    
    // 1. 检查容器格式
    QString container = metadata.containerFormat.toLower();
    if (!constraints.supportedContainers.contains(container)) {
        return FormatCompatibility::NeedsTranscode;
    }
    
    // 2. 检查视频编码
    QString codec = metadata.codecName.toLower();
    if (!constraints.supportedVideoCodecs.contains(codec)) {
        return FormatCompatibility::NeedsTranscode;
    }
    
    // 3. 检查像素格式
    QString pixelFmt = metadata.pixelFormat.toLower();
    if (!constraints.supportedPixelFormats.contains(pixelFmt)) {
        // 特殊处理：10-bit HDR
        if (pixelFmt.contains("10le") || pixelFmt.contains("p010")) {
            return FormatCompatibility::NeedsToneMapping;
        }
        // 其他格式需要转换
        return FormatCompatibility::NeedsColorConvert;
    }
    
    // 4. 检查位深
    if (metadata.bitDepth > constraints.maxBitDepth) {
        return FormatCompatibility::NeedsColorConvert;
    }
    
    // 5. 检查HDR
    if (metadata.isHDR && !constraints.supportsHDR) {
        return FormatCompatibility::NeedsToneMapping;
    }
    
    // 6. 检查Alpha通道
    if (metadata.hasAlpha && !constraints.supportsAlpha) {
        return FormatCompatibility::PartiallySupported;
    }
    
    return FormatCompatibility::FullySupported;
}

VideoCompatibilityReport VideoCompatibilityAnalyzer::analyze(
    const QString& videoPath) const
{
    VideoCompatibilityReport report;
    
    // 提取视频元数据
    VideoMetadata metadata = extractMetadata(videoPath);
    
    // 分析尺寸兼容性
    report.sizeCompatibility = analyzeSize(metadata.size(), m_impl->modelInfo);
    report.originalSize = metadata.size();
    report.adaptedSize = calculateAdaptedSize(metadata.size(), report.sizeCompatibility);
    
    // 分析格式兼容性
    report.formatCompatibility = analyzeFormat(metadata);
    report.containerFormat = metadata.containerFormat;
    report.videoCodec = metadata.codecName;
    report.pixelFormat = metadata.pixelFormat;
    report.isHDR = metadata.isHDR;
    report.hasAlpha = metadata.hasAlpha;
    report.bitDepth = metadata.bitDepth;
    report.frameRate = metadata.frameRate;
    
    // 综合评估
    report.canProcess = (report.sizeCompatibility != SizeCompatibility::Incompatible &&
                         report.formatCompatibility != FormatCompatibility::NotSupported);
    
    report.requiresUserConfirmation = 
        (report.sizeCompatibility == SizeCompatibility::Incompatible ||
         report.formatCompatibility == FormatCompatibility::PartiallySupported);
    
    // 生成警告和错误信息
    generateWarningsAndErrors(report);
    
    // 估算资源需求
    report.estimatedMemoryMB = estimateMemoryUsage(report.adaptedSize, m_impl->modelInfo);
    report.estimatedGpuMemoryMB = estimateGpuMemoryUsage(report.adaptedSize, m_impl->modelInfo);
    report.estimatedProcessingTimeMs = estimateProcessingTime(metadata, m_impl->modelInfo);
    
    return report;
}
```

### 4.4 HDR色调映射算法

```cpp
QImage VideoFormatConverter::applyToneMapping(const QImage& hdrFrame, 
                                               double peakLuminance) const
{
    if (hdrFrame.isNull()) return QImage();
    
    const int w = hdrFrame.width();
    const int h = hdrFrame.height();
    
    QImage sdrFrame(w, h, QImage::Format_RGB888);
    
    // Reinhard色调映射算法
    const double L_white = peakLuminance;
    const double exposure = 1.0;
    
    for (int y = 0; y < h; ++y) {
        const QRgb* srcLine = reinterpret_cast<const QRgb*>(hdrFrame.constScanLine(y));
        uchar* dstLine = sdrFrame.scanLine(y);
        
        for (int x = 0; x < w; ++x) {
            QRgb hdrPixel = srcLine[x];
            
            // 提取HDR值（假设已经归一化到0-1范围）
            double r = qRed(hdrPixel) / 255.0;
            double g = qGreen(hdrPixel) / 255.0;
            double b = qBlue(hdrPixel) / 255.0;
            
            // 计算亮度
            double L = 0.2126 * r + 0.7152 * g + 0.0722 * b;
            
            // Reinhard压缩
            double L_d = (L * (1.0 + L / (L_white * L_white))) / (1.0 + L);
            double scale = L_d / std::max(L, 1e-6);
            
            // 应用到RGB通道
            r = std::min(1.0, r * scale * exposure);
            g = std::min(1.0, g * scale * exposure);
            b = std::min(1.0, b * scale * exposure);
            
            // Gamma校正
            r = std::pow(r, 1.0 / 2.2);
            g = std::pow(g, 1.0 / 2.2);
            b = std::pow(b, 1.0 / 2.2);
            
            // 输出SDR值
            dstLine[x * 3 + 0] = static_cast<uchar>(std::clamp(r * 255.0, 0.0, 255.0));
            dstLine[x * 3 + 1] = static_cast<uchar>(std::clamp(g * 255.0, 0.0, 255.0));
            dstLine[x * 3 + 2] = static_cast<uchar>(std::clamp(b * 255.0, 0.0, 255.0));
        }
    }
    
    return sdrFrame;
}
```

### 4.5 帧适配与还原算法

```cpp
QImage VideoSizeAdapter::adaptFrame(const QImage& frame, 
                                     const SizeAdaptationResult& adaptation) const
{
    if (adaptation.compatibility == SizeCompatibility::FullyCompatible ||
        adaptation.compatibility == SizeCompatibility::NeedsTiling) {
        return frame.copy();
    }
    
    const QSize& targetSize = adaptation.adaptedSize;
    QImage adapted;
    
    switch (adaptation.compatibility) {
    case SizeCompatibility::NeedsPadding: {
        adapted = QImage(targetSize, frame.format());
        adapted.fill(Qt::black);
        
        QPainter painter(&adapted);
        int x = (targetSize.width() - frame.width()) / 2;
        int y = (targetSize.height() - frame.height()) / 2;
        painter.drawImage(x, y, frame);
        painter.end();
        break;
    }
    
    case SizeCompatibility::NeedsDownscale: {
        adapted = frame.scaled(targetSize, Qt::KeepAspectRatio, 
                               Qt::SmoothTransformation);
        break;
    }
    
    default:
        adapted = frame.copy();
        break;
    }
    
    return adapted;
}

QImage VideoSizeAdapter::restoreFrame(const QImage& processed, 
                                       const SizeAdaptationResult& adaptation) const
{
    if (adaptation.compatibility == SizeCompatibility::FullyCompatible ||
        adaptation.compatibility == SizeCompatibility::NeedsTiling) {
        return processed.copy();
    }
    
    const QSize& originalSize = adaptation.originalSize;
    const QSize& adaptedSize = adaptation.adaptedSize;
    QImage restored;
    
    switch (adaptation.compatibility) {
    case SizeCompatibility::NeedsPadding: {
        int x = (adaptedSize.width() - originalSize.width()) / 2 
                * adaptation.outputSize.width() / adaptedSize.width();
        int y = (adaptedSize.height() - originalSize.height()) / 2 
                * adaptation.outputSize.height() / adaptedSize.height();
        int w = originalSize.width() * adaptation.outputSize.width() / adaptedSize.width();
        int h = originalSize.height() * adaptation.outputSize.height() / adaptedSize.height();
        restored = processed.copy(x, y, w, h);
        break;
    }
    
    case SizeCompatibility::NeedsDownscale: {
        QSize outputOriginal = QSize(
            originalSize.width() * m_impl->modelInfo.scaleFactor,
            originalSize.height() * m_impl->modelInfo.scaleFactor);
        restored = processed.scaled(outputOriginal, Qt::KeepAspectRatio, 
                                    Qt::SmoothTransformation);
        break;
    }
    
    default:
        restored = processed.copy();
        break;
    }
    
    return restored;
}
```

---

## 5. 集成方案

### 5.1 修改 AIVideoProcessor

```cpp
// 扩展 AIVideoProcessor::processInternal

void AIVideoProcessor::processInternal(const QString& inputPath, 
                                        const QString& outputPath)
{
    // ... 现有初始化代码 ...
    
    // 新增：综合兼容性分析
    VideoCompatibilityAnalyzer analyzer;
    analyzer.setModelInfo(m_impl->modelInfo);
    analyzer.setSizeConstraints(VideoCompatibilityAnalyzer::getDefaultSizeConstraints());
    analyzer.setFormatConstraints(VideoCompatibilityAnalyzer::getDefaultFormatConstraints());
    
    VideoCompatibilityReport report = analyzer.analyze(inputPath);
    
    // 记录分析结果
    qInfo() << "[AIVideoProcessor] Compatibility analysis:"
            << "size:" << static_cast<int>(report.sizeCompatibility)
            << "format:" << static_cast<int>(report.formatCompatibility)
            << "original:" << report.originalSize
            << "adapted:" << report.adaptedSize
            << "canProcess:" << report.canProcess;
    
    // 检查是否可以处理
    if (!report.canProcess) {
        QString error = report.errors.join("\n");
        qWarning() << "[AIVideoProcessor] Cannot process video:" << error;
        emitCompleted(false, QString(), error);
        return;
    }
    
    // 发出警告信息
    if (!report.warnings.isEmpty()) {
        emit warning(report.warnings.join("\n"));
    }
    
    // 如果需要用户确认，等待确认
    if (report.requiresUserConfirmation) {
        // TODO: 实现用户确认机制
        emit warning(tr("视频需要特殊处理: %1").arg(report.formatAdaptationReason));
    }
    
    // 创建适配器
    VideoSizeAdapter sizeAdapter;
    sizeAdapter.setModelInfo(m_impl->modelInfo);
    sizeAdapter.setConstraints(VideoCompatibilityAnalyzer::getDefaultSizeConstraints());
    
    VideoFormatConverter formatConverter;
    
    // 更新配置
    if (report.recommendedTileSize > 0) {
        m_impl->config.tileSize = report.recommendedTileSize;
    }
    
    // 更新输出尺寸
    const int outW = report.adaptedSize.width() * m_impl->modelInfo.scaleFactor & ~1;
    const int outH = report.adaptedSize.height() * m_impl->modelInfo.scaleFactor & ~1;
    
    // ... 后续处理流程 ...
    
    // 在帧处理循环中应用适配
    while (guard.readFrame(pkt)) {
        // ... 解码帧 ...
        
        // 1. 格式转换（HDR、色彩空间等）
        QImage convertedFrame = frameImg;
        if (report.formatCompatibility == FormatCompatibility::NeedsToneMapping) {
            convertedFrame = formatConverter.applyToneMapping(frameImg);
        } else if (report.formatCompatibility == FormatCompatibility::NeedsColorConvert) {
            convertedFrame = formatConverter.convertPixelFormat(frameImg, srcFmt, AV_PIX_FMT_RGB24);
        }
        
        // 2. 尺寸适配
        QImage adaptedFrame = sizeAdapter.adaptFrame(convertedFrame, report);
        
        // 3. AI推理
        QImage resultImg = processFrame(adaptedFrame);
        
        // 4. 还原尺寸
        if (resultImg.isNull()) {
            resultImg = frameImg;
        } else {
            resultImg = sizeAdapter.restoreFrame(resultImg, report);
        }
        
        // ... 编码输出 ...
    }
}
```

### 5.2 新增文件清单

| 文件 | 用途 |
|------|------|
| `include/EnhanceVision/core/video/VideoCompatibilityAnalyzer.h` | 兼容性分析器头文件 |
| `src/core/video/VideoCompatibilityAnalyzer.cpp` | 兼容性分析器实现 |
| `include/EnhanceVision/core/video/VideoSizeAdapter.h` | 尺寸适配器头文件 |
| `src/core/video/VideoSizeAdapter.cpp` | 尺寸适配器实现 |
| `include/EnhanceVision/core/video/VideoFormatConverter.h` | 格式转换器头文件 |
| `src/core/video/VideoFormatConverter.cpp` | 格式转换器实现 |
| `include/EnhanceVision/core/video/VideoFrameBuffer.h` | 帧缓冲池头文件 |
| `src/core/video/VideoFrameBuffer.cpp` | 帧缓冲池实现 |

### 5.3 修改文件清单

| 文件 | 修改内容 |
|------|----------|
| `include/EnhanceVision/core/video/VideoProcessingTypes.h` | 扩展VideoProcessingConfig、VideoMetadata |
| `include/EnhanceVision/core/video/AIVideoProcessor.h` | 添加分析器和适配器成员 |
| `src/core/video/AIVideoProcessor.cpp` | 集成兼容性分析和适配流程 |
| `src/core/video/VideoResourceGuard.cpp` | 添加HDR/色彩空间检测 |
| `CMakeLists.txt` | 添加新源文件 |

---

## 6. 线程安全设计

### 6.1 互斥锁策略

```cpp
struct AIVideoProcessor::Impl {
    // 现有成员
    AIEngine* aiEngine = nullptr;
    ModelInfo modelInfo;
    VideoProcessingConfig config;
    
    // 新增：尺寸适配器（线程安全）
    std::unique_ptr<VideoSizeAdapter> sizeAdapter;
    
    // 线程同步
    std::atomic<bool> isProcessing{false};
    std::atomic<bool> cancelled{false};
    std::atomic<double> progress{0.0};
    
    QMutex mutex;  // 保护 modelInfo, config 的读写
};
```

### 6.2 资源管理

- **RAII原则**：所有FFmpeg资源通过VideoResourceGuard管理
- **智能指针**：使用std::unique_ptr管理适配器生命周期
- **原子操作**：进度和取消标志使用std::atomic

---

## 7. 错误处理与恢复

### 7.1 错误类型定义

```cpp
enum class VideoProcessingError {
    None,
    FileOpenFailed,
    DecoderInitFailed,
    EncoderInitFailed,
    SizeIncompatible,
    MemoryAllocationFailed,
    GPUOomError,
    FrameProcessingFailed,
    OutputWriteFailed,
    UserCancelled
};

struct VideoProcessingResult {
    bool success;
    VideoProcessingError error;
    QString errorMessage;
    QString outputPath;
    int processedFrames;
    int failedFrames;
    qint64 processingTimeMs;
};
```

### 7.2 错误恢复策略

| 错误类型 | 恢复策略 |
|----------|----------|
| SizeIncompatible | 自动调整尺寸或提示用户 |
| FormatNotSupported | 自动转码或提示用户 |
| HDRNotSupported | 应用色调映射转换 |
| MemoryAllocationFailed | 降低并发帧数，重试 |
| GPUOomError | 切换CPU模式或减小分块 |
| FrameProcessingFailed | 跳过失败帧，继续处理 |
| ColorSpaceMismatch | 自动转换色彩空间 |

---

## 8. 性能优化措施

### 8.1 内存优化

- **帧缓冲池**：限制最大缓冲帧数，避免内存无限增长
- **及时释放**：处理完的帧立即释放
- **预估内存**：处理前预估内存需求，提前预警

### 8.2 GPU优化

- **智能分块**：根据GPU显存动态调整分块大小
- **OOM降级**：GPU OOM时自动切换到更小分块或CPU模式
- **定期清理**：每处理N帧清理一次GPU缓存

### 8.3 UI流畅性

- **异步处理**：所有耗时操作在工作线程执行
- **进度节流**：限制进度更新频率（100ms间隔）
- **状态缓存**：避免重复计算

### 8.4 格式转换优化

- **硬件加速**：优先使用NVENC/NVDEC进行编解码
- **零拷贝**：尽量使用GPU内存直接处理，避免CPU-GPU数据传输
- **批量处理**：对HDR色调映射等操作批量处理

---

## 9. 测试计划

### 9.1 单元测试

| 测试项 | 测试内容 |
|--------|----------|
| VideoCompatibilityAnalyzer | 尺寸和格式兼容性判断逻辑 |
| VideoSizeAdapter | 帧适配和还原的正确性 |
| VideoFormatConverter | 色彩空间转换、HDR色调映射 |
| VideoFrameBuffer | 线程安全性、边界条件 |

### 9.2 集成测试

| 测试场景 | 预期结果 |
|----------|----------|
| 标准尺寸视频（1920x1080） | 直接处理，无需适配 |
| 超大视频（4K/8K） | 自动分块处理 |
| 异常宽高比视频 | 自动填充处理 |
| HDR视频（HDR10/HLG） | 自动色调映射 |
| 10-bit视频 | 正确转换到8-bit |
| GPU OOM场景 | 自动降级恢复 |
| 非标准帧率视频 | 保持音画同步 |

### 9.3 兼容性测试矩阵

| 视频格式 | 分辨率 | 编码 | 像素格式 | 预期行为 |
|----------|--------|------|----------|----------|
| MP4 | 1080p | H.264 | yuv420p | 直接处理 |
| MP4 | 4K | H.265 | yuv420p10le | 色调映射+分块 |
| MOV | 1080p | ProRes | yuv422p | 色彩转换 |
| MKV | 1080p | VP9 | yuv420p | 直接处理 |
| WebM | 720p | AV1 | yuv420p | 直接处理 |
| AVI | 480p | MPEG4 | yuv420p | 直接处理 |

### 9.4 性能测试

| 指标 | 基线要求 |
|------|----------|
| 内存峰值 | < 4GB |
| GPU显存峰值 | < 2GB |
| UI响应延迟 | < 100ms |
| 处理速度 | >= 5fps（1080p视频） |
| HDR转换开销 | < 10ms/帧 |

---

## 10. 实施步骤

### Phase 1: 基础设施（2-3天）

1. 创建VideoCompatibilityAnalyzer类框架
2. 实现尺寸和格式检测算法
3. 扩展VideoMetadata结构
4. 编写单元测试

### Phase 2: 核心功能（3-4天）

1. 实现VideoSizeAdapter帧适配和还原算法
2. 实现VideoFormatConverter格式转换
3. 实现HDR色调映射算法
4. 创建VideoFrameBuffer类

### Phase 3: 集成与优化（3-4天）

1. 修改AIVideoProcessor集成分析器和适配器
2. 实现错误处理和恢复机制
3. 性能优化和内存管理
4. 硬件加速集成

### Phase 4: 测试与验证（2-3天）

1. 编写集成测试
2. 兼容性测试矩阵验证
3. 性能测试和调优
4. 文档更新

---

## 11. 风险评估

| 风险 | 影响 | 缓解措施 |
|------|------|----------|
| 尺寸调整导致质量下降 | 中 | 使用高质量缩放算法，保持宽高比 |
| HDR色调映射效果不佳 | 中 | 提供多种色调映射算法选项 |
| GPU OOM恢复失败 | 高 | 多级降级策略，最终CPU回退 |
| 线程竞争问题 | 高 | 完善锁策略，充分测试 |
| 格式兼容性问题 | 中 | 广泛测试各种视频格式，建立兼容性矩阵 |
| 10-bit精度丢失 | 低 | 使用浮点中间格式处理 |
| 音画不同步 | 中 | 基于PTS时间戳同步，处理帧率异常 |
| 性能下降 | 中 | 硬件加速，优化算法 |

---

## 12. 总结

本方案通过引入视频兼容性分析系统，实现了视频尺寸和格式的智能检测与适配：

### 12.1 核心能力

1. **综合兼容性分析**：处理前自动分析视频尺寸、格式与AI模型兼容性
2. **智能尺寸适配**：根据不同情况选择填充、缩放或分块策略
3. **格式转换支持**：正确处理HDR、10-bit、色彩空间等格式问题
4. **质量保证**：保持内容完整性，推理质量满足要求
5. **健壮性**：完善的错误处理和恢复机制

### 12.2 技术亮点

- **VideoCompatibilityAnalyzer**：统一的兼容性分析入口
- **VideoSizeAdapter**：智能尺寸适配与还原
- **VideoFormatConverter**：HDR色调映射、色彩空间转换
- **VideoFrameBuffer**：线程安全的帧缓冲池

### 12.3 架构遵循

方案遵循项目现有架构规范：
- 采用RAII资源管理
- 线程安全设计
- 与现有AIEngine分块处理和GPU OOM降级机制无缝集成
- 符合项目分层架构（Core层）

### 12.4 预期效果

- 支持更多视频格式和分辨率
- 自动处理HDR和10-bit视频
- 提升用户体验，减少处理失败
- 保持高性能和低内存占用
