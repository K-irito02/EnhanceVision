/**
 * @file AIImageProcessor.h
 * @brief AI 图像处理器
 * @author EnhanceVision Team
 * 
 * 使用推理后端执行图像处理任务。
 * 支持单图、分块和 TTA 处理模式。
 */

#ifndef ENHANCEVISION_AIIMAGEPROCESSOR_H
#define ENHANCEVISION_AIIMAGEPROCESSOR_H

#include "EnhanceVision/core/inference/IInferenceBackend.h"
#include "EnhanceVision/core/inference/ImagePreprocessor.h"
#include "EnhanceVision/core/inference/ImagePostprocessor.h"
#include "EnhanceVision/core/inference/AutoParamCalculator.h"
#include "EnhanceVision/models/DataTypes.h"
#include <QObject>
#include <QImage>
#include <memory>

namespace EnhanceVision {

/**
 * @brief AI 图像处理器
 * 
 * 使用推理后端执行图像处理任务。
 * 支持单图、分块和 TTA 处理模式。
 */
class AIImageProcessor : public QObject
{
    Q_OBJECT

public:
    explicit AIImageProcessor(QObject* parent = nullptr);
    ~AIImageProcessor() override;
    
    /**
     * @brief 设置推理后端
     * @param backend 推理后端实例
     */
    void setBackend(IInferenceBackend* backend);
    
    /**
     * @brief 处理图像
     * @param input 输入图像
     * @param model 模型信息
     * @param params 处理参数
     * @return 处理结果
     */
    InferenceResult process(const QImage& input, 
                            const ModelInfo& model,
                            const QVariantMap& params = QVariantMap());
    
    /**
     * @brief 取消当前处理
     */
    void cancel();
    
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
    void completed(const InferenceResult& result);

private:
    InferenceResult processSingle(const QImage& input, const ModelInfo& model);
    InferenceResult processTiled(const QImage& input, const ModelInfo& model, int tileSize);
    InferenceResult processWithTTA(const QImage& input, const ModelInfo& model);
    
    IInferenceBackend* m_backend = nullptr;
    std::atomic<bool> m_processing{false};
    std::atomic<double> m_progress{0.0};
};

} // namespace EnhanceVision

#endif // ENHANCEVISION_AIIMAGEPROCESSOR_H
