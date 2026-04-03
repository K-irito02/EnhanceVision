/**
 * @file AIModelManager.h
 * @brief AI 模型管理器
 * @author EnhanceVision Team
 * 
 * 统一管理模型加载、卸载和缓存。
 * 从 AIEngine 中提取的模型管理逻辑。
 */

#ifndef ENHANCEVISION_AIMODELMANAGER_H
#define ENHANCEVISION_AIMODELMANAGER_H

#include "EnhanceVision/core/inference/IInferenceBackend.h"
#include "EnhanceVision/models/DataTypes.h"
#include <QObject>
#include <QMutex>
#include <QHash>
#include <memory>

namespace EnhanceVision {

class ModelRegistry;

/**
 * @brief AI 模型管理器
 * 
 * 统一管理模型加载、卸载和缓存。
 * 支持模型预加载和自动卸载。
 */
class AIModelManager : public QObject
{
    Q_OBJECT

public:
    explicit AIModelManager(QObject* parent = nullptr);
    ~AIModelManager() override;
    
    /**
     * @brief 设置模型注册表
     * @param registry 模型注册表
     */
    void setModelRegistry(ModelRegistry* registry);
    
    /**
     * @brief 设置推理后端
     * @param backend 推理后端
     */
    void setBackend(IInferenceBackend* backend);
    
    /**
     * @brief 加载模型
     * @param modelId 模型 ID
     * @return true 如果加载成功
     */
    bool loadModel(const QString& modelId);
    
    /**
     * @brief 异步加载模型
     * @param modelId 模型 ID
     */
    void loadModelAsync(const QString& modelId);
    
    /**
     * @brief 卸载当前模型
     */
    void unloadModel();
    
    /**
     * @brief 预加载模型（加载但不立即使用）
     * @param modelId 模型 ID
     */
    void preloadModel(const QString& modelId);
    
    /**
     * @brief 检查模型是否已加载
     * @param modelId 模型 ID
     */
    bool isModelLoaded(const QString& modelId) const;
    
    /**
     * @brief 获取当前模型 ID
     */
    QString currentModelId() const;
    
    /**
     * @brief 获取当前模型信息
     */
    ModelInfo currentModel() const;
    
    /**
     * @brief 获取模型信息
     * @param modelId 模型 ID
     * @return 模型信息
     */
    ModelInfo getModelInfo(const QString& modelId) const;
    
    /**
     * @brief 清除所有缓存
     */
    void clearCache();

signals:
    void modelLoaded(bool success, const QString& modelId);
    void modelUnloaded();
    void errorOccurred(const QString& error);

private:
    ModelRegistry* m_modelRegistry = nullptr;
    IInferenceBackend* m_backend = nullptr;
    
    QString m_currentModelId;
    ModelInfo m_currentModel;
    
    QHash<QString, ModelInfo> m_preloadedModels;
    
    mutable QMutex m_mutex;
};

} // namespace EnhanceVision

#endif // ENHANCEVISION_AIMODELMANAGER_H
