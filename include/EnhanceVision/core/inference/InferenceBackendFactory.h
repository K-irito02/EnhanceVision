/**
 * @file InferenceBackendFactory.h
 * @brief 推理后端工厂
 * @author EnhanceVision Team
 * 
 * 提供创建推理后端的工厂方法。
 * 支持自动选择最佳后端和手动指定后端类型。
 */

#ifndef ENHANCEVISION_INFERENCEBACKENDFACTORY_H
#define ENHANCEVISION_INFERENCEBACKENDFACTORY_H

#include "InferenceTypes.h"
#include "IInferenceBackend.h"
#include <memory>
#include <QList>

namespace EnhanceVision {

/**
 * @brief 推理后端工厂
 * 
 * 提供创建推理后端的静态方法。
 * 支持查询可用后端和自动选择最佳后端。
 */
class InferenceBackendFactory
{
public:
    /**
     * @brief 创建推理后端
     * @param type 后端类型
     * @return 后端实例，如果创建失败返回 nullptr
     */
    static std::unique_ptr<IInferenceBackend> create(BackendType type = BackendType::Auto);
    
    /**
     * @brief 获取所有可用的后端类型
     * @return 可用的后端类型列表
     */
    static QList<BackendType> availableBackends();
    
    /**
     * @brief 获取推荐的后端类型
     * @return 推荐的后端类型
     * 
     * 选择逻辑：
     * 1. 优先选择 GPU 后端（如果可用）
     * 2. 其次选择 CPU 后端
     */
    static BackendType recommendedBackend();
    
    /**
     * @brief 检查后端是否可用
     * @param type 后端类型
     * @return true 如果后端可用
     */
    static bool isBackendAvailable(BackendType type);
    
    /**
     * @brief 获取后端信息
     * @param type 后端类型
     * @return 后端描述信息
     */
    static QString backendInfo(BackendType type);
    
    /**
     * @brief 注册自定义后端
     * @param type 后端类型
     * @param creator 创建函数
     * @return true 如果注册成功
     */
    using BackendCreator = std::function<std::unique_ptr<IInferenceBackend>()>;
    static bool registerBackend(BackendType type, BackendCreator creator);

private:
    static QMap<BackendType, BackendCreator>& registry();
};

} // namespace EnhanceVision

#endif // ENHANCEVISION_INFERENCEBACKENDFACTORY_H
