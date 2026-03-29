/**
 * @file VideoSizeAdapter.h
 * @brief 视频尺寸适配器
 * @author EnhanceVision Team
 */

#ifndef ENHANCEVISION_VIDEOSIZEADAPTER_H
#define ENHANCEVISION_VIDEOSIZEADAPTER_H

#include "VideoCompatibilityAnalyzer.h"
#include <QObject>
#include <QImage>
#include <QSize>
#include <memory>

namespace EnhanceVision {

struct SizeAdaptationResult {
    SizeCompatibility compatibility = SizeCompatibility::FullyCompatible;
    QSize originalSize;
    QSize adaptedSize;
    QSize outputSize;
    int recommendedTileSize = 0;
    QString adaptationReason;
    bool requiresUserConfirmation = false;
    double estimatedMemoryMB = 0.0;
    double estimatedGpuMemoryMB = 0.0;
};

class VideoSizeAdapter : public QObject {
    Q_OBJECT
    
public:
    explicit VideoSizeAdapter(QObject* parent = nullptr);
    ~VideoSizeAdapter() override;
    
    void setModelInfo(const ModelInfo& model);
    void setConstraints(const SizeConstraints& constraints);
    void setGpuOomDetected(bool detected);
    
    SizeAdaptationResult analyze(const QSize& videoSize) const;
    QImage adaptFrame(const QImage& frame, const SizeAdaptationResult& adaptation) const;
    QImage restoreFrame(const QImage& processed, const SizeAdaptationResult& adaptation) const;
    
    static SizeConstraints getDefaultConstraints();
    static SizeConstraints getConstraintsForModel(const ModelInfo& model);
    
signals:
    void adaptationRequired(const QString& reason, const QSize& original, const QSize& adapted);
    
private:
    int computeOptimalTileSize(const QSize& videoSize) const;
    double estimateMemoryUsage(const QSize& size) const;
    double estimateGpuMemoryUsage(const QSize& size) const;
    
    struct Impl;
    std::unique_ptr<Impl> m_impl;
};

} // namespace EnhanceVision

#endif // ENHANCEVISION_VIDEOSIZEADAPTER_H
