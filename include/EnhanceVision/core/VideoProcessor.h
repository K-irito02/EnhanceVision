/**
 * @file VideoProcessor.h
 * @brief 视频处理器
 * @author Qt客户端开发工程师
 */

#ifndef ENHANCEVISION_VIDEOPROCESSOR_H
#define ENHANCEVISION_VIDEOPROCESSOR_H

#include <QObject>
#include <QString>
#include <memory>
#include <atomic>
#include "EnhanceVision/models/DataTypes.h"

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/avutil.h>
#include <libavutil/imgutils.h>
#include <libswscale/swscale.h>
}

namespace EnhanceVision {

class ProgressReporter;

enum class VideoProcessingStage {
    Idle = 0,
    Opening,
    DecodingInit,
    EncodingInit,
    FrameProcessing,
    EncodingFinalize,
    Writing,
    Completed
};

class VideoProcessor : public QObject
{
    Q_OBJECT

public:
    explicit VideoProcessor(QObject *parent = nullptr);
    ~VideoProcessor() override;

    void processVideoAsync(const QString& inputPath,
                          const QString& outputPath,
                          const ShaderParams& params,
                          ProgressCallback progressCallback = nullptr,
                          FinishCallback finishCallback = nullptr);

    void processVideoAsyncWithReporter(const QString& inputPath,
                                       const QString& outputPath,
                                       const ShaderParams& params,
                                       ProgressReporter* reporter,
                                       ProgressCallback progressCallback = nullptr,
                                       FinishCallback finishCallback = nullptr);

    void cancel();
    bool isProcessing() const;
    VideoProcessingStage currentStage() const;

signals:
    void progressChanged(int progress, const QString& status);
    void finished(bool success, const QString& resultPath, const QString& error);
    void stageChanged(VideoProcessingStage stage);
    void cancelled();

private:
    void processVideoInternal(const QString& inputPath,
                             const QString& outputPath,
                             const ShaderParams& params,
                             ProgressReporter* reporter,
                             ProgressCallback progressCallback,
                             FinishCallback finishCallback);
    
    void reportStageProgress(VideoProcessingStage stage, double stageProgress);
    QString stageToString(VideoProcessingStage stage) const;

    bool m_isProcessing;
    std::atomic<bool> m_cancelled{false};
    ProgressReporter* m_reporter;
    std::atomic<VideoProcessingStage> m_currentStage{VideoProcessingStage::Idle};

    static constexpr int kProgressOpeningStart = 0;
    static constexpr int kProgressOpeningEnd = 5;
    static constexpr int kProgressDecodingInitStart = 5;
    static constexpr int kProgressDecodingInitEnd = 10;
    static constexpr int kProgressEncodingInitStart = 10;
    static constexpr int kProgressEncodingInitEnd = 15;
    static constexpr int kProgressFrameProcessingStart = 15;
    static constexpr int kProgressFrameProcessingEnd = 90;
    static constexpr int kProgressEncodingFinalizeStart = 90;
    static constexpr int kProgressEncodingFinalizeEnd = 95;
    static constexpr int kProgressWritingStart = 95;
    static constexpr int kProgressWritingEnd = 100;
};

} // namespace EnhanceVision

#endif // ENHANCEVISION_VIDEOPROCESSOR_H
