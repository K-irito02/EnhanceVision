/**
 * @file AIEngine.h
 * @brief NCNN AI 推理引擎
 * @author Qt客户端开发工程师
 */

#ifndef ENHANCEVISION_AIENGINE_H
#define ENHANCEVISION_AIENGINE_H

#include <QObject>
#include <QImage>
#include <QMutex>
#include <atomic>
#include <memory>
#include <net.h>
#if NCNN_VULKAN
#include <gpu.h>
#endif
#include "EnhanceVision/models/DataTypes.h"

namespace EnhanceVision {

class ModelRegistry;

/**
 * @brief NCNN AI 推理引擎
 *
 * 提供基于 NCNN 的 AI 推理能力，支持 Vulkan GPU 加速、
 * 异步推理、大图分块处理、进度回调和取消机制。
 */
class AIEngine : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool isProcessing READ isProcessing NOTIFY processingChanged)
    Q_PROPERTY(double progress READ progress NOTIFY progressChanged)
    Q_PROPERTY(QString currentModelId READ currentModelId NOTIFY modelChanged)
    Q_PROPERTY(bool gpuAvailable READ gpuAvailable CONSTANT)
    Q_PROPERTY(bool useGpu READ useGpu WRITE setUseGpu NOTIFY useGpuChanged)

public:
    explicit AIEngine(QObject *parent = nullptr);
    ~AIEngine() override;

    // ========== 模型管理 ==========

    /**
     * @brief 加载指定模型
     * @param modelId 模型 ID（对应 ModelRegistry 中注册的 ID）
     * @return 是否加载成功
     */
    Q_INVOKABLE bool loadModel(const QString &modelId);

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
     * @brief OpenCV 图像修复（同步）
     * @param input 输入图像
     * @param mask 掩码图像（非零像素为修复区域）
     * @param radius 修复半径
     * @param method 方法: 0=Telea, 1=Navier-Stokes
     * @return 修复后的图像
     */
    QImage inpaint(const QImage &input, const QImage &mask, int radius = 3, int method = 0);

    /**
     * @brief 异步 OpenCV 图像修复
     * @param inputPath 输入图像路径
     * @param maskPath 掩码图像路径
     * @param outputPath 输出图像路径
     * @param radius 修复半径
     * @param method 方法: 0=Telea, 1=Navier-Stokes
     */
    Q_INVOKABLE void inpaintAsync(const QString &inputPath, const QString &maskPath,
                                   const QString &outputPath, int radius = 3, int method = 0);

    // ========== 参数设置 ==========

    /**
     * @brief 设置模型参数
     */
    Q_INVOKABLE void setParameter(const QString &name, const QVariant &value);

    /**
     * @brief 获取模型参数
     */
    Q_INVOKABLE QVariant getParameter(const QString &name) const;

    /**
     * @brief 合并用户参数和模型默认参数
     * @return 合并后的参数映射
     */
    QVariantMap mergeWithDefaultParams() const;

    /**
     * @brief 获取有效参数（合并默认值并验证范围）
     * @return 有效参数映射
     */
    Q_INVOKABLE QVariantMap getEffectiveParams() const;

    // ========== 状态查询 ==========

    bool isProcessing() const;
    double progress() const;
    QString currentModelId() const;
    bool gpuAvailable() const;
    bool useGpu() const;
    void setUseGpu(bool use);

    /**
     * @brief 设置 ModelRegistry 引用
     */
    void setModelRegistry(ModelRegistry *registry);

signals:
    void modelLoaded(const QString &modelId);
    void modelUnloaded();
    void modelChanged();
    void processingChanged(bool processing);
    void progressChanged(double progress);
    void processCompleted(const QImage &result);
    void processFileCompleted(bool success, const QString &resultPath, const QString &error);
    void processError(const QString &error);
    void useGpuChanged(bool useGpu);

private:
    // ========== 内部方法 ==========
    void initVulkan();
    void destroyVulkan();
    void updateOptions();

    ncnn::Mat qimageToMat(const QImage &image, const ModelInfo &model);
    QImage matToQimage(const ncnn::Mat &mat, const ModelInfo &model);

    QImage processTiled(const QImage &input, const ModelInfo &model);
    QImage processSingle(const QImage &input, const ModelInfo &model);
    ncnn::Mat runInference(const ncnn::Mat &input, const ModelInfo &model);

    void setProgress(double value);
    void setProcessing(bool processing);

    // ========== 成员变量 ==========
    ncnn::Net m_net;
#if NCNN_VULKAN
    ncnn::VulkanDevice *m_vkdev = nullptr;
#endif
    ncnn::Option m_opt;

    ModelRegistry *m_modelRegistry = nullptr;
    QString m_currentModelId;
    ModelInfo m_currentModel;
    QVariantMap m_parameters;

    std::atomic<bool> m_isProcessing{false};
    std::atomic<double> m_progress{0.0};
    std::atomic<bool> m_cancelRequested{false};
    bool m_gpuAvailable = false;
    bool m_useGpu = true;

    mutable QMutex m_mutex;
};

} // namespace EnhanceVision

#endif // ENHANCEVISION_AIENGINE_H
