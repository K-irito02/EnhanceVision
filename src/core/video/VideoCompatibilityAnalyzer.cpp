/**
 * @file VideoCompatibilityAnalyzer.cpp
 * @brief 视频兼容性分析器实现
 * @author EnhanceVision Team
 */

#include "EnhanceVision/core/video/VideoCompatibilityAnalyzer.h"
#include "EnhanceVision/models/DataTypes.h"
#include <QDebug>
#include <QFileInfo>
#include <cmath>

extern "C" {
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <libavutil/pixdesc.h>
}

namespace EnhanceVision {

struct VideoCompatibilityAnalyzer::Impl {
    ModelInfo modelInfo;
    SizeConstraints sizeConstraints;
    FormatConstraints formatConstraints;
    bool gpuOomDetected = false;
};

VideoCompatibilityAnalyzer::VideoCompatibilityAnalyzer(QObject* parent)
    : QObject(parent)
    , m_impl(std::make_unique<Impl>())
{
    m_impl->sizeConstraints = getDefaultSizeConstraints();
    m_impl->formatConstraints = getDefaultFormatConstraints();
}

VideoCompatibilityAnalyzer::~VideoCompatibilityAnalyzer() = default;

void VideoCompatibilityAnalyzer::setModelInfo(const ModelInfo& model)
{
    m_impl->modelInfo = model;
    m_impl->sizeConstraints = getSizeConstraintsForModel(model);
}

void VideoCompatibilityAnalyzer::setSizeConstraints(const SizeConstraints& constraints)
{
    m_impl->sizeConstraints = constraints;
}

void VideoCompatibilityAnalyzer::setFormatConstraints(const FormatConstraints& constraints)
{
    m_impl->formatConstraints = constraints;
}

SizeConstraints VideoCompatibilityAnalyzer::getDefaultSizeConstraints()
{
    return SizeConstraints{};
}

FormatConstraints VideoCompatibilityAnalyzer::getDefaultFormatConstraints()
{
    return FormatConstraints{};
}

SizeConstraints VideoCompatibilityAnalyzer::getSizeConstraintsForModel(const ModelInfo& model)
{
    SizeConstraints constraints = getDefaultSizeConstraints();
    
    if (model.tileSize > 0) {
        constraints.maxPixelsForSinglePass = 
            static_cast<qint64>(model.tileSize) * model.tileSize;
    }
    
    return constraints;
}

VideoCompatibilityReport VideoCompatibilityAnalyzer::analyze(const QString& videoPath) const
{
    VideoCompatibilityReport report;
    
    AVFormatContext* fmtCtx = nullptr;
    if (avformat_open_input(&fmtCtx, videoPath.toUtf8().constData(), nullptr, nullptr) < 0) {
        report.canProcess = false;
        report.errors.append(tr("无法打开视频文件: %1").arg(videoPath));
        return report;
    }
    
    if (avformat_find_stream_info(fmtCtx, nullptr) < 0) {
        avformat_close_input(&fmtCtx);
        report.canProcess = false;
        report.errors.append(tr("无法获取视频流信息"));
        return report;
    }
    
    VideoMetadata metadata;
    metadata.durationMs = fmtCtx->duration / 1000;
    
    int videoIdx = -1;
    for (unsigned int i = 0; i < fmtCtx->nb_streams; ++i) {
        if (fmtCtx->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_VIDEO) {
            videoIdx = static_cast<int>(i);
            break;
        }
    }
    
    if (videoIdx < 0) {
        avformat_close_input(&fmtCtx);
        report.canProcess = false;
        report.errors.append(tr("未找到视频流"));
        return report;
    }
    
    auto* codecPar = fmtCtx->streams[videoIdx]->codecpar;
    metadata.width = codecPar->width;
    metadata.height = codecPar->height;
    
    const AVCodecDescriptor* codecDesc = avcodec_descriptor_get(codecPar->codec_id);
    if (codecDesc) {
        metadata.codecName = QString::fromUtf8(codecDesc->name);
    }
    
    if (fmtCtx->iformat) {
        metadata.format = detectVideoFormat(QString::fromUtf8(fmtCtx->iformat->name));
    }
    
    const AVPixFmtDescriptor* pixDesc = av_pix_fmt_desc_get(
        static_cast<AVPixelFormat>(codecPar->format));
    if (pixDesc) {
        metadata.pixelFormat = QString::fromUtf8(pixDesc->name);
        report.bitDepth = pixDesc->comp[0].depth;
        report.hasAlpha = pixDesc->flags & AV_PIX_FMT_FLAG_ALPHA;
    }
    
    if (codecPar->color_primaries == AVCOL_PRI_BT2020 ||
        codecPar->color_trc == AVCOL_TRC_SMPTE2084 ||
        codecPar->color_trc == AVCOL_TRC_ARIB_STD_B67) {
        report.isHDR = true;
    }
    
    AVRational frameRate = fmtCtx->streams[videoIdx]->avg_frame_rate;
    if (frameRate.num > 0 && frameRate.den > 0) {
        metadata.frameRate = av_q2d(frameRate);
    }
    
    metadata.totalFrames = fmtCtx->streams[videoIdx]->nb_frames;
    
    avformat_close_input(&fmtCtx);
    
    report = analyze(metadata);
    report.containerFormat = QFileInfo(videoPath).suffix().toLower();
    
    return report;
}

VideoCompatibilityReport VideoCompatibilityAnalyzer::analyze(const VideoMetadata& metadata) const
{
    VideoCompatibilityReport report;
    
    report.originalSize = metadata.size();
    report.videoCodec = metadata.codecName;
    report.pixelFormat = metadata.pixelFormat;
    report.frameRate = metadata.frameRate;
    report.bitDepth = metadata.bitDepth > 0 ? metadata.bitDepth : 8;
    
    report.sizeCompatibility = analyzeSize(metadata.size());
    report.formatCompatibility = analyzeFormat(metadata);
    
    report.adaptedSize = report.originalSize;
    if (report.sizeCompatibility == SizeCompatibility::NeedsPadding) {
        const double aspectRatio = static_cast<double>(report.originalSize.width()) / 
                                   report.originalSize.height();
        if (aspectRatio > m_impl->sizeConstraints.maxAspectRatio) {
            report.adaptedSize.setHeight(static_cast<int>(
                report.originalSize.width() / m_impl->sizeConstraints.maxAspectRatio));
        } else if (aspectRatio < m_impl->sizeConstraints.minAspectRatio) {
            report.adaptedSize.setWidth(static_cast<int>(
                report.originalSize.height() * m_impl->sizeConstraints.minAspectRatio));
        }
        
        if (m_impl->sizeConstraints.alignmentMultiple > 1) {
            int align = m_impl->sizeConstraints.alignmentMultiple;
            report.adaptedSize.setWidth(
                ((report.adaptedSize.width() + align - 1) / align) * align);
            report.adaptedSize.setHeight(
                ((report.adaptedSize.height() + align - 1) / align) * align);
        }
    } else if (report.sizeCompatibility == SizeCompatibility::NeedsDownscale) {
        const qint64 pixels = static_cast<qint64>(report.originalSize.width()) * 
                              report.originalSize.height();
        double scale = std::sqrt(
            static_cast<double>(m_impl->sizeConstraints.maxPixelsForSinglePass) / pixels);
        report.adaptedSize.setWidth(static_cast<int>(report.originalSize.width() * scale));
        report.adaptedSize.setHeight(static_cast<int>(report.originalSize.height() * scale));
    }
    
    if (report.sizeCompatibility == SizeCompatibility::NeedsTiling) {
        report.recommendedTileSize = computeOptimalTileSize(report.originalSize);
    }
    
    int scaleFactor = m_impl->modelInfo.scaleFactor > 0 ? m_impl->modelInfo.scaleFactor : 1;
    report.outputSize = QSize(
        report.adaptedSize.width() * scaleFactor,
        report.adaptedSize.height() * scaleFactor);
    
    report.canProcess = (report.sizeCompatibility != SizeCompatibility::Incompatible &&
                         report.formatCompatibility != FormatCompatibility::NotSupported);
    
    report.requiresUserConfirmation = 
        (report.sizeCompatibility == SizeCompatibility::Incompatible ||
         report.formatCompatibility == FormatCompatibility::PartiallySupported);
    
    report.estimatedMemoryMB = estimateMemoryUsage(report.adaptedSize);
    report.estimatedGpuMemoryMB = estimateGpuMemoryUsage(report.adaptedSize);
    report.estimatedProcessingTimeMs = estimateProcessingTime(metadata);
    
    generateWarningsAndErrors(report);
    
    return report;
}

SizeCompatibility VideoCompatibilityAnalyzer::analyzeSize(const QSize& size) const
{
    const int w = size.width();
    const int h = size.height();
    
    if (w <= 0 || h <= 0) {
        return SizeCompatibility::Incompatible;
    }
    
    const auto& constraints = m_impl->sizeConstraints;
    
    if (w < constraints.minWidth || h < constraints.minHeight) {
        return SizeCompatibility::Incompatible;
    }
    
    if (w > constraints.maxWidth || h > constraints.maxHeight) {
        return SizeCompatibility::NeedsDownscale;
    }
    
    const double aspectRatio = static_cast<double>(w) / h;
    if (aspectRatio > constraints.maxAspectRatio || 
        aspectRatio < constraints.minAspectRatio) {
        return SizeCompatibility::NeedsPadding;
    }
    
    const qint64 pixels = static_cast<qint64>(w) * h;
    if (pixels > constraints.maxPixelsForSinglePass) {
        if (m_impl->modelInfo.tileSize > 0) {
            return SizeCompatibility::NeedsTiling;
        }
        return SizeCompatibility::NeedsDownscale;
    }
    
    return SizeCompatibility::FullyCompatible;
}

FormatCompatibility VideoCompatibilityAnalyzer::analyzeFormat(const VideoMetadata& metadata) const
{
    const auto& constraints = m_impl->formatConstraints;
    
    QString codec = metadata.codecName.toLower();
    if (!constraints.supportedVideoCodecs.contains(codec)) {
        return FormatCompatibility::NeedsTranscode;
    }
    
    QString pixelFmt = metadata.pixelFormat.toLower();
    if (!pixelFmt.isEmpty() && !constraints.supportedPixelFormats.contains(pixelFmt)) {
        if (pixelFmt.contains("10le") || pixelFmt.contains("p010") ||
            pixelFmt.contains("p016")) {
            return FormatCompatibility::NeedsToneMapping;
        }
        return FormatCompatibility::NeedsColorConvert;
    }
    
    if (metadata.bitDepth > constraints.maxBitDepth) {
        return FormatCompatibility::NeedsColorConvert;
    }
    
    return FormatCompatibility::FullySupported;
}

double VideoCompatibilityAnalyzer::estimateMemoryUsage(const QSize& size) const
{
    const qint64 pixels = static_cast<qint64>(size.width()) * size.height();
    const int channels = 3;
    const int scaleFactor = m_impl->modelInfo.scaleFactor > 0 ? 
                            m_impl->modelInfo.scaleFactor : 1;
    
    double inputMB = static_cast<double>(pixels * channels) / (1024 * 1024);
    double outputMB = static_cast<double>(pixels * scaleFactor * scaleFactor * channels) / 
                      (1024 * 1024);
    double intermediateMB = inputMB * 4;
    
    return inputMB + outputMB + intermediateMB;
}

double VideoCompatibilityAnalyzer::estimateGpuMemoryUsage(const QSize& size) const
{
    const qint64 pixels = static_cast<qint64>(size.width()) * size.height();
    const int channels = 3;
    const int scaleFactor = m_impl->modelInfo.scaleFactor > 0 ? 
                            m_impl->modelInfo.scaleFactor : 1;
    
    double kFactor = 8.0;
    const int layerCount = m_impl->modelInfo.layerCount;
    
    if (layerCount > 500) {
        kFactor = 48.0;
    } else if (layerCount > 200) {
        kFactor = 32.0;
    } else if (layerCount > 50) {
        kFactor = 16.0;
    } else if (layerCount > 0) {
        kFactor = 10.0;
    }
    
    double baseMB = static_cast<double>(pixels * channels * sizeof(float) * 
                                        scaleFactor * scaleFactor) / (1024 * 1024);
    
    return baseMB * kFactor;
}

qint64 VideoCompatibilityAnalyzer::estimateProcessingTime(const VideoMetadata& metadata) const
{
    if (metadata.totalFrames <= 0 || metadata.frameRate <= 0) {
        return 0;
    }
    
    double msPerFrame = 100.0;
    
    const qint64 pixels = static_cast<qint64>(metadata.width) * metadata.height;
    if (pixels > 2073600) {
        msPerFrame *= 2.0;
    } else if (pixels > 8294400) {
        msPerFrame *= 4.0;
    }
    
    return static_cast<qint64>(metadata.totalFrames * msPerFrame);
}

void VideoCompatibilityAnalyzer::generateWarningsAndErrors(VideoCompatibilityReport& report) const
{
    switch (report.sizeCompatibility) {
    case SizeCompatibility::FullyCompatible:
        report.sizeAdaptationReason = tr("视频尺寸完全兼容，无需调整");
        break;
    case SizeCompatibility::NeedsPadding:
        report.sizeAdaptationReason = tr("Abnormal aspect ratio, needs padding");
        report.warnings.append(report.sizeAdaptationReason);
        break;
    case SizeCompatibility::NeedsDownscale:
        report.sizeAdaptationReason = tr("Video size exceeds safe range, needs downscaling");
        report.warnings.append(report.sizeAdaptationReason);
        break;
    case SizeCompatibility::NeedsTiling:
        report.sizeAdaptationReason = tr("视频尺寸较大，将使用分块处理");
        report.warnings.append(report.sizeAdaptationReason);
        break;
    case SizeCompatibility::Incompatible:
        report.sizeAdaptationReason = tr("视频尺寸不兼容，无法处理");
        report.errors.append(report.sizeAdaptationReason);
        break;
    }
    
    switch (report.formatCompatibility) {
    case FormatCompatibility::FullySupported:
        report.formatAdaptationReason = tr("视频格式完全支持");
        break;
    case FormatCompatibility::NeedsTranscode:
        report.formatAdaptationReason = tr("Video codec requires transcoding");
        report.warnings.append(report.formatAdaptationReason);
        break;
    case FormatCompatibility::NeedsColorConvert:
        report.formatAdaptationReason = tr("Pixel format conversion required");
        report.warnings.append(report.formatAdaptationReason);
        break;
    case FormatCompatibility::NeedsToneMapping:
        report.formatAdaptationReason = tr("HDR视频需要色调映射");
        report.warnings.append(report.formatAdaptationReason);
        break;
    case FormatCompatibility::PartiallySupported:
        report.formatAdaptationReason = tr("视频格式部分支持，可能丢失信息");
        report.warnings.append(report.formatAdaptationReason);
        break;
    case FormatCompatibility::NotSupported:
        report.formatAdaptationReason = tr("视频格式不支持");
        report.errors.append(report.formatAdaptationReason);
        break;
    }
    
    if (report.isHDR) {
        report.warnings.append(tr("HDR video detected, applying tone mapping"));
    }
    
    if (report.bitDepth > 8) {
        report.warnings.append(tr("%1-bit video detected, converting to 8-bit").arg(report.bitDepth));
    }
}

int VideoCompatibilityAnalyzer::computeOptimalTileSize(const QSize& videoSize) const
{
    const int w = videoSize.width();
    const int h = videoSize.height();
    
    double kFactor = 8.0;
    const int layerCount = m_impl->modelInfo.layerCount;
    
    if (layerCount > 500) {
        kFactor = 48.0;
    } else if (layerCount > 200) {
        kFactor = 32.0;
    } else if (layerCount > 50) {
        kFactor = 16.0;
    } else if (layerCount > 0) {
        kFactor = 10.0;
    }
    
    qint64 availableMB = 1024;
    if (m_impl->gpuOomDetected) {
        availableMB = 256;
    }
    
    const int channels = m_impl->modelInfo.outputChannels > 0 ? 
                         m_impl->modelInfo.outputChannels : 3;
    const int scale = std::max(1, m_impl->modelInfo.scaleFactor);
    
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

} // namespace EnhanceVision
