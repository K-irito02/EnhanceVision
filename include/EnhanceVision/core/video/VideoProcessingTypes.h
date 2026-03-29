/**
 * @file VideoProcessingTypes.h
 * @brief AI 推理视频处理类型定义
 * @author EnhanceVision Team
 */

#ifndef ENHANCEVISION_VIDEOPROCESSINGTYPES_H
#define ENHANCEVISION_VIDEOPROCESSINGTYPES_H

#include <QString>
#include <QSize>

namespace EnhanceVision {

enum class VideoAspectRatio {
    Ratio_16_9,
    Ratio_4_3,
    Ratio_1_1,
    Ratio_9_16,
    Ratio_21_9,
    Ratio_Custom
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
    QString pixelFormat;
    int bitDepth = 8;
    bool isHDR = false;
    bool hasAlpha = false;
    
    QSize size() const { return QSize(width, height); }
    bool isValid() const { return width > 0 && height > 0 && totalFrames > 0; }
};

struct VideoProcessingConfig {
    int tileSize = 0;
    double outscale = 1.0;
    bool useGpu = true;
    int maxConcurrentFrames = 2;
    int gpuCleanupInterval = 10;
    QString outputCodec = "h264";
    int quality = 18;
    int outputWidth = 0;
    int outputHeight = 0;
    
    enum class SizeAdaptationPolicy {
        Auto,
        AlwaysAdapt,
        NeverAdapt,
        UserConfirm
    };
    
    SizeAdaptationPolicy sizeAdaptationPolicy = SizeAdaptationPolicy::Auto;
    bool preserveAspectRatio = true;
    int maxOutputWidth = 4096;
    int maxOutputHeight = 4096;
    bool enableSmartTiling = true;
    int safetyMarginPercent = 10;
};

inline VideoAspectRatio detectAspectRatio(int width, int height) {
    if (width <= 0 || height <= 0) return VideoAspectRatio::Ratio_Custom;
    
    double ratio = static_cast<double>(width) / height;
    const double epsilon = 0.05;
    
    if (qAbs(ratio - 16.0/9.0) < epsilon) return VideoAspectRatio::Ratio_16_9;
    if (qAbs(ratio - 4.0/3.0) < epsilon) return VideoAspectRatio::Ratio_4_3;
    if (qAbs(ratio - 1.0) < epsilon) return VideoAspectRatio::Ratio_1_1;
    if (qAbs(ratio - 9.0/16.0) < epsilon) return VideoAspectRatio::Ratio_9_16;
    if (qAbs(ratio - 21.0/9.0) < epsilon) return VideoAspectRatio::Ratio_21_9;
    
    return VideoAspectRatio::Ratio_Custom;
}

inline VideoFormat detectVideoFormat(const QString& extension) {
    QString ext = extension.toLower();
    if (ext == "mp4") return VideoFormat::MP4;
    if (ext == "avi") return VideoFormat::AVI;
    if (ext == "mov" || ext == "qt") return VideoFormat::MOV;
    if (ext == "mkv" || ext == "webm") return VideoFormat::MKV;
    if (ext == "flv") return VideoFormat::FLV;
    return VideoFormat::Unknown;
}

inline QString videoFormatExtension(VideoFormat format) {
    switch (format) {
        case VideoFormat::MP4: return "mp4";
        case VideoFormat::AVI: return "avi";
        case VideoFormat::MOV: return "mov";
        case VideoFormat::MKV: return "mkv";
        case VideoFormat::WebM: return "webm";
        case VideoFormat::FLV: return "flv";
        default: return "mp4";
    }
}

} // namespace EnhanceVision

#endif // ENHANCEVISION_VIDEOPROCESSINGTYPES_H
