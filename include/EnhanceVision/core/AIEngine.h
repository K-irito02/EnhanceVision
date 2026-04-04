/**
 * @file AIEngine.h
 * @brief NCNN AI 推理引擎（支持 CPU / Vulkan GPU 双模式）
 * @author EnhanceVision Team
 */

#ifndef ENHANCEVISION_AIENGINE_H
#define ENHANCEVISION_AIENGINE_H

#include <QObject>
#include <QImage>
#include <QMutex>
#include <QElapsedTimer>
#include <QPointer>
#include <atomic>
#include <memory>
#include <mutex>
#include <condition_variable>
#include <net.h>
#include "EnhanceVision/core/inference/InferenceTypes.h"
#include "EnhanceVision/models/DataTypes.h"

#ifdef NCNN_VULKAN_AVAILABLE
#include <gpu.h>
#endif

namespace EnhanceVision {

class ModelRegistry;
class ProgressReporter;

/**
 * @brief NCNN AI 推理引擎（支持 CPU / Vulkan GPU 双模式）
 *
 * 提供基于 NCNN 的 AI 推理功能，支持 CPU 和 Vulkan GPU 两种后端。
 * 支持异步推理、大图分块处理、进度回调和取消机制。
 */
class AIEngine : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool isProcessing READ isProcessing NOTIFY processingChanged)
    Q_PROPERTY(double progress READ progress NOTIFY progressChanged)
    Q_PROPERTY(QString progressText READ progressText NOTIFY progressTextChanged)
    Q_PROPERTY(QString currentModelId READ currentModelId NOTIFY modelChanged)
    Q_PROPERTY(BackendType backendType READ backendType NOTIFY backendTypeChanged)
    Q_PROPERTY(bool gpuAvailable READ gpuAvailable NOTIFY gpuDeviceInfoChanged)
    Q_PROPERTY(QString gpuDeviceName READ gpuDeviceName NOTIFY gpuDeviceInfoChanged)

public:
    explicit AIEngine(QObject *parent = nullptr);
    ~AIEngine() override;

    // ========== 模型管理 ==========

    Q_INVOKABLE bool loadModel(const QString &modelId);
    Q_INVOKABLE void loadModelAsync(const QString &modelId);
    Q_INVOKABLE void unloadModel();

    // ========== 推理接口 ==========

    QImage process(const QImage &input);
    Q_INVOKABLE void processAsync(const QString &inputPath, const QString &outputPath);
    Q_INVOKABLE void cancelProcess();
    void resetCancelFlag();
    Q_INVOKABLE void forceCancel();
    bool isForceCancelled() const;
    Q_INVOKABLE void resetState();
    Q_INVOKABLE void safeCleanup();
    void clearInferenceCache();

    QImage inpaint(const QImage &input, const QImage &mask, int radius = 3, int method = 0);
    void inpaintAsync(const QString &inputPath, const QString &maskPath,
                      const QString &outputPath, int radius = 3, int method = 0);

    // ========== 参数管理 ==========

    Q_INVOKABLE void setParameter(const QString &name, const QVariant &value);
    Q_INVOKABLE QVariant getParameter(const QString &name) const;
    Q_INVOKABLE void clearParameters();
    QVariantMap mergeWithDefaultParams() const;
    QVariantMap getEffectiveParams() const;

    // ========== 自动参数计算 ==========

    Q_INVOKABLE int computeAutoTileSize(const QSize &inputSize) const;
    Q_INVOKABLE QVariantMap computeAutoParams(const QSize &mediaSize, bool isVideo) const;

    // ========== 后端类型管理 ==========

    /**
     * @brief 设置推理后端类型（CPU 或 Vulkan GPU）
     * @param type 目标后端类型
     * @return true 如果切换成功
     */
    Q_INVOKABLE bool setBackendType(BackendType type);

    /**
     * @brief 启动时探测 Vulkan GPU 可用性（不占用 GPU 资源）
     * 在事件循环启动后自动调用，探测结果通过 gpuDeviceInfoChanged 信号通知 UI
     */
    Q_INVOKABLE void probeGpuAvailability();

    BackendType backendType() const { return m_backendType; }

    /**
     * @brief 检查 Vulkan GPU 是否可用
     */
    bool gpuAvailable() const { return m_gpuAvailable.load(); }

    /**
     * @brief 获取 GPU 设备名称
     */
    QString gpuDeviceName() const { return m_gpuDeviceName; }

    /**
     * @brief 初始化 Vulkan GPU 资源
     * @param gpuDeviceId GPU 设备索引（默认 0）
     * @return true 如果初始化成功
     */
    bool initializeVulkan(int gpuDeviceId = 0);

    /**
     * @brief 释放 Vulkan GPU 资源
     */
    void shutdownVulkan();

    /**
     * @brief 检查 Vulkan 是否就绪可用
     */
    bool isVulkanReady() const;

    /**
     * @brief 等待模型加载同步完成
     * @param timeoutMs 超时时间（毫秒）
     * @return true 如果同步完成，false 如果超时
     */
    bool waitForModelSyncComplete(int timeoutMs = 5000);

    /**
     * @brief 等待 GPU 就绪
     * @param timeoutMs 超时时间（毫秒）
     * @return true 如果 GPU 就绪，false 如果超时
     */
    bool waitForGpuReady(int timeoutMs = 5000);

    // ========== 状态查询 ==========

    bool isProcessing() const;
    double progress() const;
    QString progressText() const;
    QString currentModelId() const;

    void setModelRegistry(ModelRegistry *registry);

    ProgressReporter* progressReporter();

signals:
    void modelLoaded(const QString &modelId);
    void modelLoadCompleted(bool success, const QString &modelId, const QString &error);
    void modelUnloaded();
    void modelChanged();
    void processingChanged(bool processing);
    void progressChanged(double progress);
    void progressTextChanged(const QString &text);
    void processCompleted(const QImage &result);
    void processFileCompleted(bool success, const QString &resultPath, const QString &error);
    void processError(const QString &error);
    void autoParamsComputed(int autoTileSize);
    void allAutoParamsComputed(const QVariantMap &autoParams);
    void backendTypeChanged(BackendType type);
    void gpuDeviceInfoChanged();

private:
    void setProgress(double value, bool forceEmit = false);
    void setProcessing(bool processing);
    void emitError(const QString &error);

    void startProgressSimulation(double fromProgress, double toProgress, qint64 estimatedMs);
    void stopProgressSimulation();
    bool isSimulatingProgress() const;

    ncnn::Mat qimageToMat(const QImage &image, const ModelInfo &model);
    QImage matToQimage(const ncnn::Mat &mat, const ModelInfo &model);
    ncnn::Mat runInference(const ncnn::Mat &input, const ModelInfo &model);

    QImage processSingle(const QImage &input, const ModelInfo &model);
    QImage processTiled(const QImage &input, const ModelInfo &model);
    QImage processWithTTA(const QImage &input, const ModelInfo &model);
    QImage mergeTTAResults(const QList<QImage> &results);
    QImage applyOutscale(const QImage &input, double scale);

    QVariantMap getEffectiveParamsLocked(const ModelInfo &model) const;
    QVariantMap computeAutoParamsForModel(const QSize &mediaSize, bool isVideo,
                                          const ModelInfo &model,
                                          const QString &modelId) const;
    int computeAutoTileSizeForModel(const QSize &inputSize, const ModelInfo &model) const;

    bool applyBackendOptions(BackendType target_type);

    ModelRegistry *m_modelRegistry = nullptr;

    ncnn::Net m_net;
    ncnn::Option m_opt;

    ModelInfo m_currentModel;
    QString m_currentModelId;

    QVariantMap m_parameters;

    std::atomic<bool> m_isProcessing{false};
    std::atomic<bool> m_cancelRequested{false};
    std::atomic<bool> m_forceCancelled{false};
    std::atomic<double> m_progress{0.0};
    std::atomic<qint64> m_lastProgressEmitMs{0};
    QString m_progressText;

    mutable QMutex m_mutex;
    mutable QMutex m_inferenceMutex;
    mutable QMutex m_paramsMutex;
    mutable QMutex m_lastErrorMutex;
    QString m_lastError;

    std::unique_ptr<ProgressReporter> m_progressReporter;

    std::atomic<bool> m_simulatingProgress{false};
    QElapsedTimer m_inferStartTime;
    double m_simulateStartProgress = 0.0;
    double m_simulateTargetProgress = 0.80;
    std::atomic<qint64> m_simulateEstimatedMs{3000};

    // ========== 后端类型相关成员 ==========
    BackendType m_backendType = BackendType::NCNN_CPU;
    std::atomic<bool> m_gpuAvailable{false};
    QString m_gpuDeviceName;
    int m_gpuDeviceId = -1;
    bool m_vulkanInstanceCreated = false;

    // ========== Vulkan 实例单例化管理 ==========
    static std::once_flag s_gpuInstanceOnceFlag;
    static std::atomic<int> s_gpuInstanceRefCount;
    static std::mutex s_gpuInstanceMutex;
    static std::atomic<bool> s_gpuInstanceCreated;
    
    static void ensureGpuInstanceCreated();
    static void releaseGpuInstanceRef();
    static bool isGpuInstanceCreated();

    // ========== 模型加载同步 ==========
    std::atomic<bool> m_modelSyncComplete{true};
    std::mutex m_modelSyncMutex;
    std::condition_variable m_modelSyncCv;

    // ========== GPU 就绪状态 ==========
    std::atomic<bool> m_gpuReady{false};
    std::mutex m_gpuReadyMutex;
    std::condition_variable m_gpuReadyCv;
    
    // ========== 模型加载时的后端类型 ==========
    BackendType m_modelLoadedBackendType = BackendType::NCNN_CPU;
};

} // namespace EnhanceVision

#endif // ENHANCEVISION_AIENGINE_H
