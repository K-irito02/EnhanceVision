/**
 * @file AIVideoProcessor.h
 * @brief AI 推理视频处理器
 * @author EnhanceVision Team
 */

#ifndef ENHANCEVISION_AIVIDEOPROCESSOR_H
#define ENHANCEVISION_AIVIDEOPROCESSOR_H

#include "VideoProcessingTypes.h"
#include "VideoResourceGuard.h"
#include "EnhanceVision/core/ProgressReporter.h"
#include <QObject>
#include <QImage>
#include <QMutex>
#include <QAtomicInt>
#include <QFuture>
#include <atomic>
#include <memory>

namespace EnhanceVision {

class AIEngine;
struct ModelInfo;

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
    void waitForFinished(int timeoutMs = 5000);
    
    bool isProcessing() const;
    double progress() const;
    
signals:
    void progressChanged(double progress);
    void processingChanged(bool processing);
    void completed(bool success, const QString& resultPath, const QString& error);
    void warning(const QString& message);
    
private:
    void processInternal(const QString& inputPath, const QString& outputPath);
    QImage processFrame(const QImage& input);
    void updateProgress(double progress, const QString& stage = QString());
    void setProcessing(bool processing);
    void emitCompleted(bool success, const QString& resultPath, const QString& error);
    double estimateProgressByTime(int frameCount, qint64 elapsedMs);
    
    struct Impl;
    std::unique_ptr<Impl> m_impl;
    std::unique_ptr<ProgressReporter> m_progressReporter;
    QFuture<void> m_processingFuture;
};

} // namespace EnhanceVision

#endif // ENHANCEVISION_AIVIDEOPROCESSOR_H
