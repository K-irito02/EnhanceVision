/**
 * @file NCNNCPUBackend.h
 * @brief NCNN CPU 推理后端
 * @author EnhanceVision Team
 */

#ifndef ENHANCEVISION_NCNNCPUBACKEND_H
#define ENHANCEVISION_NCNNCPUBACKEND_H

#include "EnhanceVision/core/inference/IInferenceBackend.h"
#include "EnhanceVision/core/inference/ImagePreprocessor.h"
#include "EnhanceVision/core/inference/ImagePostprocessor.h"
#include <net.h>
#include <QMutex>
#include <atomic>

namespace EnhanceVision {

class NCNNCPUBackend : public IInferenceBackend
{
    Q_OBJECT

public:
    explicit NCNNCPUBackend(QObject* parent = nullptr);
    ~NCNNCPUBackend() override;
    
    bool initialize(const BackendConfig& config) override;
    void shutdown() override;
    
    bool loadModel(const ModelInfo& model) override;
    void unloadModel() override;
    bool isModelLoaded() const override;
    QString currentModelId() const override;
    ModelInfo currentModel() const override;
    
    InferenceResult inference(const InferenceRequest& request) override;
    
    BackendType type() const override { return BackendType::NCNN_CPU; }
    BackendCapabilities capabilities() const override;
    QString name() const override { return QStringLiteral("NCNN CPU Backend"); }
    bool isInferenceRunning() const override;

private:
    InferenceResult processSingle(const QImage& input);
    InferenceResult processTiled(const QImage& input, int tileSize);
    InferenceResult processWithTTA(const QImage& input);
    
    ncnn::Mat runInference(const ncnn::Mat& input);
    
    void setProgress(double value);
    void emitError(const QString& error, int code = -1);
    void resetCancelFlag();
    bool isCancelRequested() const;
    void setInferenceRunning(bool running);

    ncnn::Net m_net;
    ncnn::Option m_opt;
    
    ModelInfo m_currentModel;
    QString m_currentModelId;
    bool m_modelLoaded = false;
    std::atomic<bool> m_isInitialized{false};
    
    BackendConfig m_config;
    
    std::atomic<bool> m_cancelRequested{false};
    std::atomic<double> m_progress{0.0};
    std::atomic<bool> m_isInferenceRunning{false};

    mutable QMutex m_mutex;
    mutable QMutex m_inferenceMutex;

signals:
    void progressChanged(double progress);
    void modelLoaded(bool success, const QString& modelId);
    void errorOccurred(const QString& error, int code);
    void inferenceCompleted(const InferenceResult& result);
};

} // namespace EnhanceVision

#endif // ENHANCEVISION_NCNNCPUBACKEND_H
