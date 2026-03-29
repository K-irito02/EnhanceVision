/**
 * @file VideoCompatibilityAnalyzer.h
 * @brief 视频兼容性分析器
 * @author EnhanceVision Team
 */

#ifndef ENHANCEVISION_VIDEOCOMPATIBILITYANALYZER_H
#define ENHANCEVISION_VIDEOCOMPATIBILITYANALYZER_H

#include "VideoProcessingTypes.h"
#include <QObject>
#include <QSize>
#include <QString>
#include <QStringList>
#include <memory>

namespace EnhanceVision {

struct ModelInfo;

enum class SizeCompatibility {
    FullyCompatible,
    NeedsPadding,
    NeedsDownscale,
    NeedsTiling,
    Incompatible
};

enum class FormatCompatibility {
    FullySupported,
    NeedsTranscode,
    NeedsColorConvert,
    NeedsToneMapping,
    PartiallySupported,
    NotSupported
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

struct VideoCompatibilityReport {
    SizeCompatibility sizeCompatibility = SizeCompatibility::FullyCompatible;
    QSize originalSize;
    QSize adaptedSize;
    QSize outputSize;
    int recommendedTileSize = 0;
    QString sizeAdaptationReason;
    
    FormatCompatibility formatCompatibility = FormatCompatibility::FullySupported;
    QString containerFormat;
    QString videoCodec;
    QString pixelFormat;
    bool isHDR = false;
    bool hasAlpha = false;
    int bitDepth = 8;
    double frameRate = 0.0;
    QString formatAdaptationReason;
    
    bool requiresUserConfirmation = false;
    bool canProcess = true;
    double estimatedMemoryMB = 0.0;
    double estimatedGpuMemoryMB = 0.0;
    qint64 estimatedProcessingTimeMs = 0;
    QStringList warnings;
    QStringList errors;
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
    SizeCompatibility analyzeSize(const QSize& size) const;
    FormatCompatibility analyzeFormat(const VideoMetadata& metadata) const;
    double estimateMemoryUsage(const QSize& size) const;
    double estimateGpuMemoryUsage(const QSize& size) const;
    qint64 estimateProcessingTime(const VideoMetadata& metadata) const;
    void generateWarningsAndErrors(VideoCompatibilityReport& report) const;
    int computeOptimalTileSize(const QSize& videoSize) const;
    
    struct Impl;
    std::unique_ptr<Impl> m_impl;
};

} // namespace EnhanceVision

#endif // ENHANCEVISION_VIDEOCOMPATIBILITYANALYZER_H
