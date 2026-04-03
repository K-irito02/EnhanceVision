/**
 * @file AIVideoProcessor.h
 * @brief AI 视频处理器（重构版）
 * @author EnhanceVision Team
 * 
 * 使用推理后端执行视频处理任务。
 * 解耦与 AIEngine 的直接依赖，使用 IInferenceBackend 接口。
 */

#ifndef ENHANCEVISION_AIVIDEOPROCESSOR_V2_H
#define ENHANCEVISION_AIVIDEOPROCESSOR_V2_H

#include "EnhanceVision/core/inference/IInferenceBackend.h"
#include "EnhanceVision/models/DataTypes.h"
#include "EnhanceVision/core/ProgressReporter.h"
#include <QObject>
#include <QImage>
#include <QMutex>
#include <QAtomicInt>
#include <QFuture>
#include <atomic>
#include <memory>

namespace EnhanceVision {

class AIImageProcessor;

/**
 * @brief 视频处理配置
 */
struct VideoProcessingConfig {
    int outputFps = 0;              ///< 输出帧率 (0 = 保持原帧率)
    int outputWidth = 0;           ///< 输出宽度 (0 = 保持原宽度)
    int outputHeight = 0;          ///< 输出高度 (0 = 保持原高度)
    QString outputCodec;           ///< 输出编码器
    int outputBitrate = 0;         ///< 输出比特率 (kbps)
    bool copyAudio = true;         ///< 是否复制音频流
    bool copySubtitle = false;     ///< 是否复制字幕流
    int processEveryNthFrame = 1;  ///< 每隔 N 帧处理一次
};

/**
 * @brief AI 视频处理器
 * 
 * 使用推理后端执行视频处理任务。
 * 支持逐帧处理、进度报告和取消机制。
 */
class AIVideoProcessor : public QObject
{
    Q_OBJECT

public:
    explicit AIVideoProcessor(QObject* parent = nullptr);
    ~AIVideoProcessor() override;
    
    /**
     * @brief 设置推理后端
     * @param backend 推理后端实例
     */
    void setBackend(IInferenceBackend* backend);
    
    /**
     * @brief 设置模型信息
     * @param model 模型信息
     */
    void setModelInfo(const ModelInfo& model);
    
    /**
     * @brief 设置处理配置
     * @param config 处理配置
     */
    void setConfig(const VideoProcessingConfig& config);
    
    /**
     * @brief 异步处理视频
     * @param inputPath 输入路径
     * @param outputPath 输出路径
     */
    void processAsync(const QString& inputPath, const QString& outputPath);
    
    /**
     * @brief 取消处理
     */
    void cancel();
    
    /**
     * @brief 等待处理完成
     * @param timeoutMs 超时时间
     */
    void waitForFinished(int timeoutMs = 5000);
    
    /**
     * @brief 检查是否正在处理
     */
    bool isProcessing() const;
    
    /**
     * @brief 获取当前进度
     */
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
    
    IInferenceBackend* m_backend = nullptr;
    ModelInfo m_model;
    VideoProcessingConfig m_config;
    
    std::atomic<bool> m_processing{false};
    std::atomic<bool> m_cancelRequested{false};
    std::atomic<double> m_progress{0.0};
    
    std::unique_ptr<ProgressReporter> m_progressReporter;
    QFuture<void> m_processingFuture;
    
    mutable QMutex m_mutex;
};

} // namespace EnhanceVision

#endif // ENHANCEVISION_AIVIDEOPROCESSOR_V2_H
