/**
 * @file NCNNVulkanBackend.h
 * @brief NCNN Vulkan GPU 推理后端
 * @author EnhanceVision Team
 */

#ifndef ENHANCEVISION_NCNNVULKANBACKEND_H
#define ENHANCEVISION_NCNNVULKANBACKEND_H

#include "EnhanceVision/core/inference/IInferenceBackend.h"
#include "EnhanceVision/core/inference/ImagePreprocessor.h"
#include "EnhanceVision/core/inference/ImagePostprocessor.h"
#include <net.h>
#include <QMutex>
#include <atomic>

#ifdef NCNN_VULKAN_AVAILABLE
#include <gpu.h>
#endif

namespace EnhanceVision {

class NCNNVulkanBackend : public IInferenceBackend
{
    Q_OBJECT

public:
    explicit NCNNVulkanBackend(QObject* parent = nullptr);
    ~NCNNVulkanBackend() override;
    
    bool initialize(const BackendConfig& config) override;
    void shutdown() override;
    
    bool loadModel(const ModelInfo& model) override;
    void unloadModel() override;
    bool isModelLoaded() const override;
    QString currentModelId() const override;
    ModelInfo currentModel() const override;
    
    InferenceResult inference(const InferenceRequest& request) override;
    
    BackendType type() const override { return BackendType::NCNN_Vulkan; }
    BackendCapabilities capabilities() const override;
    QString name() const override { return QStringLiteral("NCNN Vulkan Backend"); }
    bool isInferenceRunning() const override;

    static bool isVulkanSupported();
    
    struct VulkanDeviceInfo {
        bool available = false;
        QString deviceName;
        int deviceCount = 0;
        qint64 totalMemoryMB = 0;
        QString driverVersion;
        QString errorReason;
    };
    static VulkanDeviceInfo probeDevice();

private:
    InferenceResult processSingle(const QImage& input);
    InferenceResult processTiled(const QImage& input, int tileSize);
    InferenceResult processWithTTA(const QImage& input);
    
    ncnn::Mat runInference(const ncnn::Mat& input);
    
    bool initGpuResources();
    void releaseGpuResources();
    
    void setProgress(double value);
    void emitError(const QString& error);
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

    int m_gpuDeviceId = 0;
    bool m_gpuInitialized = false;
    bool m_vulkanInstanceCreated = false;

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

#endif // ENHANCEVISION_NCNNVULKANBACKEND_H
