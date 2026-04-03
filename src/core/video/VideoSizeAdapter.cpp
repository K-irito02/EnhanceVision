/**
 * @file VideoSizeAdapter.cpp
 * @brief 视频尺寸适配器实现
 * @author EnhanceVision Team
 */

#include "EnhanceVision/core/video/VideoSizeAdapter.h"
#include "EnhanceVision/models/DataTypes.h"
#include <QPainter>
#include <cmath>

namespace EnhanceVision {

struct VideoSizeAdapter::Impl {
    ModelInfo modelInfo;
    SizeConstraints constraints;
    bool gpuOomDetected = false;
};

VideoSizeAdapter::VideoSizeAdapter(QObject* parent)
    : QObject(parent)
    , m_impl(std::make_unique<Impl>())
{
    m_impl->constraints = getDefaultConstraints();
}

VideoSizeAdapter::~VideoSizeAdapter() = default;

void VideoSizeAdapter::setModelInfo(const ModelInfo& model)
{
    m_impl->modelInfo = model;
}

void VideoSizeAdapter::setConstraints(const SizeConstraints& constraints)
{
    m_impl->constraints = constraints;
}

void VideoSizeAdapter::setGpuOomDetected(bool detected)
{
    m_impl->gpuOomDetected = detected;
}

SizeConstraints VideoSizeAdapter::getDefaultConstraints()
{
    return SizeConstraints{};
}

SizeConstraints VideoSizeAdapter::getConstraintsForModel(const ModelInfo& model)
{
    SizeConstraints constraints = getDefaultConstraints();
    
    if (model.tileSize > 0) {
        constraints.maxPixelsForSinglePass = 
            static_cast<qint64>(model.tileSize) * model.tileSize;
    }
    
    return constraints;
}

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
    
    if (w <= 0 || h <= 0) {
        result.compatibility = SizeCompatibility::Incompatible;
        result.adaptationReason = tr("视频尺寸无效");
        result.requiresUserConfirmation = true;
        return result;
    }
    
    if (w < constraints.minWidth || h < constraints.minHeight) {
        result.compatibility = SizeCompatibility::Incompatible;
        result.adaptationReason = tr("视频尺寸过小，低于模型最低要求");
        result.requiresUserConfirmation = true;
        return result;
    }
    
    if (w > constraints.maxWidth || h > constraints.maxHeight) {
        result.compatibility = SizeCompatibility::NeedsDownscale;
        double scale = std::min(
            static_cast<double>(constraints.maxWidth) / w,
            static_cast<double>(constraints.maxHeight) / h);
        result.adaptedSize = QSize(
            static_cast<int>(w * scale),
            static_cast<int>(h * scale));
        result.adaptationReason = tr("视频尺寸超出最大限制，需要缩小处理");
    }
    
    if (result.compatibility == SizeCompatibility::FullyCompatible) {
        if (aspectRatio > constraints.maxAspectRatio || 
            aspectRatio < constraints.minAspectRatio) {
            result.compatibility = SizeCompatibility::NeedsPadding;
            result.adaptationReason = tr("视频宽高比异常，需要填充处理");
            
            if (aspectRatio > constraints.maxAspectRatio) {
                int targetH = static_cast<int>(w / constraints.maxAspectRatio);
                result.adaptedSize = QSize(w, targetH);
            } else {
                int targetW = static_cast<int>(h * constraints.maxAspectRatio);
                result.adaptedSize = QSize(targetW, h);
            }
        }
    }
    
    if (constraints.alignmentMultiple > 1 && 
        result.compatibility != SizeCompatibility::NeedsDownscale) {
        int align = constraints.alignmentMultiple;
        int alignedW = ((w + align - 1) / align) * align;
        int alignedH = ((h + align - 1) / align) * align;
        if (alignedW != w || alignedH != h) {
            if (result.compatibility == SizeCompatibility::FullyCompatible) {
                result.compatibility = SizeCompatibility::NeedsPadding;
                result.adaptationReason = tr("视频尺寸需要填充以匹配模型要求");
            }
            result.adaptedSize = QSize(alignedW, alignedH);
        }
    }
    
    if (pixels > constraints.maxPixelsForSinglePass &&
        result.compatibility != SizeCompatibility::NeedsDownscale) {
        if (model.tileSize > 0 || model.layerCount > 0) {
            result.compatibility = SizeCompatibility::NeedsTiling;
            result.recommendedTileSize = computeOptimalTileSize(videoSize);
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
    
    result.estimatedMemoryMB = estimateMemoryUsage(result.adaptedSize);
    result.estimatedGpuMemoryMB = estimateGpuMemoryUsage(result.adaptedSize);
    
    int scaleFactor = model.scaleFactor > 0 ? model.scaleFactor : 1;
    result.outputSize = QSize(
        result.adaptedSize.width() * scaleFactor,
        result.adaptedSize.height() * scaleFactor);
    
    if (result.compatibility == SizeCompatibility::FullyCompatible) {
        result.adaptationReason = tr("视频尺寸完全兼容，无需调整");
    }
    
    return result;
}

QImage VideoSizeAdapter::adaptFrame(const QImage& frame, 
                                     const SizeAdaptationResult& adaptation) const
{
    qInfo() << "[VideoSizeAdapter][DEBUG] adaptFrame() called:"
            << "frameSize:" << frame.width() << "x" << frame.height()
            << "frameFormat:" << frame.format()
            << "frameIsNull:" << frame.isNull()
            << "frameBits:" << (frame.bits() != nullptr ? "valid" : "null")
            << "frameBytesPerLine:" << frame.bytesPerLine()
            << "compatibility:" << static_cast<int>(adaptation.compatibility);
    fflush(stdout);
    
    if (frame.isNull()) {
        qWarning() << "[VideoSizeAdapter] adaptFrame: input frame is null";
        return QImage();
    }
    
    // 【安全验证】检查 frame 数据有效性
    if (!frame.bits()) {
        qWarning() << "[VideoSizeAdapter] adaptFrame: frame bits is null";
        return QImage();
    }
    
    // 【安全验证】检查 frame 尺寸
    if (frame.width() <= 0 || frame.height() <= 0) {
        qWarning() << "[VideoSizeAdapter] adaptFrame: invalid frame size"
                   << frame.width() << "x" << frame.height();
        return QImage();
    }
    
    if (adaptation.compatibility == SizeCompatibility::FullyCompatible) {
        // 【关键修复】对于 FullyCompatible,直接返回传入的帧,避免任何内存操作
        // 这样可以避免在堆损坏时触发崩溃
        qInfo() << "[VideoSizeAdapter][DEBUG] adaptFrame: FullyCompatible, returning frame directly (no copy)";
        fflush(stdout);
        return frame;
    }
    
    if (adaptation.compatibility == SizeCompatibility::NeedsTiling) {
        // 【关键修复】对于 NeedsTiling,也直接返回传入的帧
        // 分块处理会在 AIEngine 中进行,这里不需要复制
        qInfo() << "[VideoSizeAdapter][DEBUG] adaptFrame: NeedsTiling, returning frame directly (no copy)";
        fflush(stdout);
        return frame;
    }
    
    const QSize& targetSize = adaptation.adaptedSize;
    if (!targetSize.isValid() || targetSize.width() <= 0 || targetSize.height() <= 0) {
        qWarning() << "[VideoSizeAdapter] adaptFrame: invalid target size" << targetSize;
        return frame.copy();
    }
    
    QImage adapted;
    
    switch (adaptation.compatibility) {
    case SizeCompatibility::NeedsPadding: {
        qInfo() << "[VideoSizeAdapter][DEBUG] adaptFrame: NeedsPadding case";
        fflush(stdout);
        
        adapted = QImage(targetSize, frame.format());
        if (adapted.isNull()) {
            qWarning() << "[VideoSizeAdapter] adaptFrame: failed to create padded image";
            return frame.copy();
        }
        adapted.fill(Qt::black);
        
        QPainter painter(&adapted);
        if (!painter.isActive()) {
            qWarning() << "[VideoSizeAdapter] adaptFrame: painter not active";
            return frame.copy();
        }
        int x = (targetSize.width() - frame.width()) / 2;
        int y = (targetSize.height() - frame.height()) / 2;
        painter.drawImage(x, y, frame);
        painter.end();
        break;
    }
    
    case SizeCompatibility::NeedsDownscale: {
        qInfo() << "[VideoSizeAdapter][DEBUG] adaptFrame: NeedsDownscale case";
        fflush(stdout);
        
        adapted = frame.scaled(targetSize, Qt::KeepAspectRatio, 
                               Qt::SmoothTransformation);
        if (adapted.isNull()) {
            qWarning() << "[VideoSizeAdapter] adaptFrame: scaled image is null";
            return frame.copy();
        }
        break;
    }
    
    default:
        qInfo() << "[VideoSizeAdapter][DEBUG] adaptFrame: default case";
        fflush(stdout);
        
        adapted = frame.copy();
        break;
    }
    
    qInfo() << "[VideoSizeAdapter][DEBUG] adaptFrame: returning adapted image"
                << "size:" << adapted.width() << "x" << adapted.height()
                << "format:" << adapted.format();
    fflush(stdout);
    
    return adapted;
}

QImage VideoSizeAdapter::restoreFrame(const QImage& processed, 
                                       const SizeAdaptationResult& adaptation) const
{
    if (processed.isNull()) {
        qWarning() << "[VideoSizeAdapter] restoreFrame: input processed is null";
        return QImage();
    }
    
    if (adaptation.compatibility == SizeCompatibility::FullyCompatible ||
        adaptation.compatibility == SizeCompatibility::NeedsTiling) {
        return processed.copy();
    }
    
    const QSize& originalSize = adaptation.originalSize;
    const QSize& adaptedSize = adaptation.adaptedSize;
    
    if (!originalSize.isValid() || !adaptedSize.isValid()) {
        qWarning() << "[VideoSizeAdapter] restoreFrame: invalid sizes"
                   << "original:" << originalSize << "adapted:" << adaptedSize;
        return processed.copy();
    }
    
    if (adaptedSize.width() <= 0 || adaptedSize.height() <= 0) {
        qWarning() << "[VideoSizeAdapter] restoreFrame: adaptedSize has zero dimension";
        return processed.copy();
    }
    
    QImage restored;
    
    switch (adaptation.compatibility) {
    case SizeCompatibility::NeedsPadding: {
        const int scaleX = adaptation.outputSize.width() / adaptedSize.width();
        const int scaleY = adaptation.outputSize.height() / adaptedSize.height();
        
        if (scaleX <= 0 || scaleY <= 0) {
            qWarning() << "[VideoSizeAdapter] restoreFrame: invalid scale factors"
                       << "scaleX:" << scaleX << "scaleY:" << scaleY;
            return processed.copy();
        }
        
        const int offsetX = (adaptedSize.width() - originalSize.width()) / 2;
        const int offsetY = (adaptedSize.height() - originalSize.height()) / 2;
        
        int x = offsetX * scaleX;
        int y = offsetY * scaleY;
        int w = originalSize.width() * scaleX;
        int h = originalSize.height() * scaleY;
        
        x = qBound(0, x, processed.width() - 1);
        y = qBound(0, y, processed.height() - 1);
        w = qMin(w, processed.width() - x);
        h = qMin(h, processed.height() - y);
        
        if (w <= 0 || h <= 0) {
            qWarning() << "[VideoSizeAdapter] restoreFrame: invalid crop rect"
                       << "x:" << x << "y:" << y << "w:" << w << "h:" << h;
            return processed.copy();
        }
        
        restored = processed.copy(x, y, w, h);
        break;
    }
    
    case SizeCompatibility::NeedsDownscale: {
        const int scaleFactor = m_impl->modelInfo.scaleFactor > 0 ? 
                                m_impl->modelInfo.scaleFactor : 1;
        const QSize outputOriginal(originalSize.width() * scaleFactor,
                                   originalSize.height() * scaleFactor);
        
        if (outputOriginal.width() <= 0 || outputOriginal.height() <= 0) {
            qWarning() << "[VideoSizeAdapter] restoreFrame: invalid output size";
            return processed.copy();
        }
        
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

int VideoSizeAdapter::computeOptimalTileSize(const QSize& videoSize) const
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
    
    auto memForTile = [channels, scale, kFactor](int tile) -> double {
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

double VideoSizeAdapter::estimateMemoryUsage(const QSize& size) const
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

double VideoSizeAdapter::estimateGpuMemoryUsage(const QSize& size) const
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

} // namespace EnhanceVision
