/**
 * @file AIEngine.h
 * @brief NCNN AI 推理引擎（纯 CPU 模式）
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
#include "EnhanceVision/models/DataTypes.h"

namespace EnhanceVision {

class ModelRegistry;
class ProgressReporter;

/**
 * @brief NCNN AI 推理引擎（纯 CPU 模式）
 *
 * 提供基于 NCNN 的 AI 推理功能，只支持 CPU 模式。
 * 支持异步推理、大图分块处理、进度回调和取消机制。
 */
class AIEngine : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool isProcessing READ isProcessing NOTIFY processingChanged)
    Q_PROPERTY(double progress READ progress NOTIFY progressChanged)
    Q_PROPERTY(QString progressText READ progressText NOTIFY progressTextChanged)
    Q_PROPERTY(QString currentModelId READ currentModelId NOTIFY modelChanged)

public:
    explicit AIEngine(QObject *parent = nullptr);
    ~AIEngine() override;

    // ========== 模型管理 ==========

    /**
     * @brief 加载指定模型（同步）
     * @param modelId 模型 ID
     * @return 是否加载成功
     */
    Q_INVOKABLE bool loadModel(const QString &modelId);

    /**
     * @brief 异步加载模型（非阻塞）
     * @param modelId 模型 ID
     */
    Q_INVOKABLE void loadModelAsync(const QString &modelId);

    /**
     * @brief 卸载当前模型
     */
    Q_INVOKABLE void unloadModel();

    // ========== 推理接口 ==========

    /**
     * @brief 同步推理（阻塞调用）
     * @param input 输入图像
     * @return 处理后的图像，失败返回空 QImage
     */
    QImage process(const QImage &input);

    /**
     * @brief 异步推理（非阻塞）
     * @param inputPath 输入图像路径
     * @param outputPath 输出图像路径
     */
    Q_INVOKABLE void processAsync(const QString &inputPath, const QString &outputPath);

    /**
     * @brief 取消正在进行的推理
     */
    Q_INVOKABLE void cancelProcess();

    /**
     * @brief 重置取消标志（供引擎池在获取引擎时调用）
     */
    void resetCancelFlag();

    /**
     * @brief 强制取消推理（立即终止）
     */
    Q_INVOKABLE void forceCancel();

    /**
     * @brief 检查是否被强制取消
     */
    bool isForceCancelled() const;

    /**
     * @brief 重置引擎状态（用于任务完成后的清理）
     */
    Q_INVOKABLE void resetState();

    /**
     * @brief 安全清理（等待推理完成后清理状态）
     */
    void safeCleanup();

    /**
     * @brief 清理推理缓存（用于视频处理时定期调用，避免内存累积）
     */
    void clearInferenceCache();

    /**
     * @brief OpenCV 图像修复（同步）
     */
    QImage inpaint(const QImage &input, const QImage &mask, int radius = 3, int method = 0);

    /**
     * @brief 异步 OpenCV 图像修复
     */
    void inpaintAsync(const QString &inputPath, const QString &maskPath,
                      const QString &outputPath, int radius = 3, int method = 0);

    // ========== 参数管理 ==========

    /**
     * @brief 设置推理参数
     */
    Q_INVOKABLE void setParameter(const QString &name, const QVariant &value);

    /**
     * @brief 获取推理参数
     */
    Q_INVOKABLE QVariant getParameter(const QString &name) const;

    /**
     * @brief 清除所有参数
     */
    Q_INVOKABLE void clearParameters();

    /**
     * @brief 合并默认参数
     */
    QVariantMap mergeWithDefaultParams() const;

    /**
     * @brief 获取有效参数（合并默认值）
     */
    QVariantMap getEffectiveParams() const;

    // ========== 自动参数计算 ==========

    /**
     * @brief 计算自动分块大小
     */
    Q_INVOKABLE int computeAutoTileSize(const QSize &inputSize) const;

    /**
     * @brief 计算所有自动参数
     */
    Q_INVOKABLE QVariantMap computeAutoParams(const QSize &mediaSize, bool isVideo) const;

    // ========== 状态查询 ==========

    bool isProcessing() const;
    double progress() const;
    QString progressText() const;
    QString currentModelId() const;

    /**
     * @brief 设置 ModelRegistry 引用
     */
    void setModelRegistry(ModelRegistry *registry);

    /**
     * @brief 获取进度报告器
     */
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

    /**
     * @brief 自动参数已计算完成
     */
    void autoParamsComputed(int autoTileSize);

    /**
     * @brief 全参数自动计算完成
     */
    void allAutoParamsComputed(const QVariantMap &autoParams);

private:
    // ========== 内部方法 ==========

    void setProgress(double value, bool forceEmit = false);
    void setProcessing(bool processing);
    void emitError(const QString &error);

    void startProgressSimulation(double fromProgress, double toProgress, qint64 estimatedMs);
    void stopProgressSimulation();
    bool isSimulatingProgress() const;

    // 图像转换
    ncnn::Mat qimageToMat(const QImage &image, const ModelInfo &model);
    QImage matToQimage(const ncnn::Mat &mat, const ModelInfo &model);

    // 推理核心
    ncnn::Mat runInference(const ncnn::Mat &input, const ModelInfo &model);

    // 处理模式
    QImage processSingle(const QImage &input, const ModelInfo &model);
    QImage processTiled(const QImage &input, const ModelInfo &model);
    QImage processWithTTA(const QImage &input, const ModelInfo &model);
    QImage mergeTTAResults(const QList<QImage> &results);
    QImage applyOutscale(const QImage &input, double scale);

    // 自动参数计算
    QVariantMap getEffectiveParamsLocked(const ModelInfo &model) const;
    QVariantMap computeAutoParamsForModel(const QSize &mediaSize, bool isVideo,
                                          const ModelInfo &model,
                                          const QString &modelId) const;
    int computeAutoTileSizeForModel(const QSize &inputSize, const ModelInfo &model) const;

    // ========== 成员变量 ==========

    ModelRegistry *m_modelRegistry = nullptr;

    // NCNN 网络和选项
    ncnn::Net m_net;
    ncnn::Option m_opt;

    // 当前模型
    ModelInfo m_currentModel;
    QString m_currentModelId;

    // 推理参数
    QVariantMap m_parameters;

    // 状态标志
    std::atomic<bool> m_isProcessing{false};
    std::atomic<bool> m_cancelRequested{false};
    std::atomic<bool> m_forceCancelled{false};
    std::atomic<double> m_progress{0.0};
    std::atomic<qint64> m_lastProgressEmitMs{0};
    QString m_progressText;

    // 互斥锁
    mutable QMutex m_mutex;
    mutable QMutex m_inferenceMutex;
    mutable QMutex m_paramsMutex;
    mutable QMutex m_lastErrorMutex;
    QString m_lastError;

    // 进度报告器
    std::unique_ptr<ProgressReporter> m_progressReporter;

    // 进度模拟器（用于 processSingle 阻塞推理期间的渐进式进度）
    std::atomic<bool> m_simulatingProgress{false};
    QElapsedTimer m_inferStartTime;
    double m_simulateStartProgress = 0.0;
    double m_simulateTargetProgress = 0.80;
    std::atomic<qint64> m_simulateEstimatedMs{3000};
};

} // namespace EnhanceVision

#endif // ENHANCEVISION_AIENGINE_H
