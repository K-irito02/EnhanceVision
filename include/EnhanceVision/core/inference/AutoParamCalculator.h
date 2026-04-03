/**
 * @file AutoParamCalculator.h
 * @brief 自动参数计算器
 * @author EnhanceVision Team
 * 
 * 从 AIEngine 中提取的自动参数计算逻辑，职责单一：
 * - 自动分块大小计算
 * - 模型参数自动计算
 * - 按模型类别的参数策略
 */

#ifndef ENHANCEVISION_AUTOPARAMCALCULATOR_H
#define ENHANCEVISION_AUTOPARAMCALCULATOR_H

#include "InferenceTypes.h"
#include "EnhanceVision/models/DataTypes.h"
#include <QSize>
#include <QVariantMap>

namespace EnhanceVision {

/**
 * @brief 自动参数计算器
 * 
 * 根据输入尺寸、模型特性和媒体类型自动计算最优参数。
 * 支持多种模型类别的参数策略。
 */
class AutoParamCalculator
{
public:
    AutoParamCalculator() = default;
    ~AutoParamCalculator() = default;
    
    // ========== 分块大小计算 ==========
    
    /**
     * @brief 计算自动分块大小
     * @param inputSize 输入尺寸
     * @param model 模型信息
     * @return 计算出的分块大小，0 表示不需要分块
     */
    static int computeTileSize(const QSize& inputSize, const ModelInfo& model);
    
    /**
     * @brief 计算自动分块大小（带内存限制）
     * @param inputSize 输入尺寸
     * @param model 模型信息
     * @param maxMemoryMB 最大可用内存 (MB)
     * @return 计算出的分块大小
     */
    static int computeTileSizeWithMemoryLimit(const QSize& inputSize, 
                                               const ModelInfo& model,
                                               qint64 maxMemoryMB);
    
    // ========== 模型参数计算 ==========
    
    /**
     * @brief 计算所有自动参数
     * @param mediaSize 媒体尺寸
     * @param isVideo 是否为视频
     * @param model 模型信息
     * @return 自动计算的参数映射
     */
    static QVariantMap computeAutoParams(const QSize& mediaSize, 
                                          bool isVideo,
                                          const ModelInfo& model);
    
    /**
     * @brief 合并用户参数和默认参数
     * @param userParams 用户提供的参数
     * @param model 模型信息
     * @return 合并后的有效参数
     */
    static QVariantMap mergeWithDefaultParams(const QVariantMap& userParams,
                                               const ModelInfo& model);
    
    // ========== 按模型类别的参数策略 ==========
    
    /**
     * @brief 计算超分辨率模型参数
     */
    static QVariantMap computeSuperResolutionParams(const QSize& mediaSize,
                                                     bool isVideo,
                                                     const ModelInfo& model);
    
    /**
     * @brief 计算去噪模型参数
     */
    static QVariantMap computeDenoisingParams(const QSize& mediaSize,
                                               bool isVideo,
                                               const ModelInfo& model);
    
    /**
     * @brief 计算去模糊模型参数
     */
    static QVariantMap computeDeblurringParams(const QSize& mediaSize,
                                                bool isVideo,
                                                const ModelInfo& model);
    
    /**
     * @brief 计算去雾模型参数
     */
    static QVariantMap computeDehazingParams(const QSize& mediaSize,
                                              bool isVideo,
                                              const ModelInfo& model);
    
    /**
     * @brief 计算上色模型参数
     */
    static QVariantMap computeColorizationParams(const QSize& mediaSize,
                                                  bool isVideo,
                                                  const ModelInfo& model);
    
    /**
     * @brief 计算低光增强模型参数
     */
    static QVariantMap computeLowLightParams(const QSize& mediaSize,
                                              bool isVideo,
                                              const ModelInfo& model);
    
    /**
     * @brief 计算插帧模型参数
     */
    static QVariantMap computeFrameInterpolationParams(const QSize& mediaSize,
                                                        bool isVideo,
                                                        const ModelInfo& model);
    
    /**
     * @brief 计算图像修复模型参数
     */
    static QVariantMap computeInpaintingParams(const QSize& mediaSize,
                                                bool isVideo,
                                                const ModelInfo& model);

private:
    /**
     * @brief 限制参数值在有效范围内
     * @param params 参数映射
     * @param key 参数键
     * @param rawValue 原始值
     * @return 限制后的值
     */
    static double clampParam(const QVariantMap& params,
                              const QString& key,
                              double rawValue);
    
    /**
     * @brief 计算动态填充大小
     * @param model 模型信息
     * @return 填充大小
     */
    static int computeDynamicPadding(const ModelInfo& model);
};

} // namespace EnhanceVision

#endif // ENHANCEVISION_AUTOPARAMCALCULATOR_H
