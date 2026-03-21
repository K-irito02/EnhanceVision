/**
 * @file ModelRegistry.h
 * @brief AI 模型注册与发现系统
 * @author Qt客户端开发工程师
 */

#ifndef ENHANCEVISION_MODELREGISTRY_H
#define ENHANCEVISION_MODELREGISTRY_H

#include <QObject>
#include <QMap>
#include <QVariantList>
#include <QVariantMap>
#include "EnhanceVision/models/DataTypes.h"

namespace EnhanceVision {

/**
 * @brief 模型注册与发现系统
 *
 * 负责从 models.json 加载模型元数据，验证模型文件可用性，
 * 提供按类别查询、按 ID 查询等接口供 AIEngine 和 QML 使用。
 */
class ModelRegistry : public QObject
{
    Q_OBJECT
    Q_PROPERTY(int modelCount READ modelCount NOTIFY modelsChanged)

public:
    explicit ModelRegistry(QObject *parent = nullptr);
    ~ModelRegistry() override;

    /**
     * @brief 获取单例实例
     */
    static ModelRegistry* instance();

    /**
     * @brief 初始化模型注册表
     * @param modelsRootPath 模型根目录路径
     * @return 成功加载的模型数量
     */
    int initialize(const QString &modelsRootPath);

    /**
     * @brief 获取指定 ID 的模型信息
     * @param modelId 模型 ID
     * @return 模型信息，若不存在返回空 ModelInfo
     */
    ModelInfo getModelInfo(const QString &modelId) const;

    /**
     * @brief 获取指定类别的所有模型
     * @param category 模型类别
     * @return 模型信息列表
     */
    QList<ModelInfo> getModelsByCategory(ModelCategory category) const;

    /**
     * @brief 获取所有可用模型（文件存在）
     * @return 模型信息列表
     */
    QList<ModelInfo> getAvailableModels() const;

    /**
     * @brief 获取所有已注册模型
     * @return 模型信息列表
     */
    QList<ModelInfo> getAllModels() const;

    /**
     * @brief 检查模型是否存在
     * @param modelId 模型 ID
     * @return 是否存在
     */
    bool hasModel(const QString &modelId) const;

    /**
     * @brief 获取模型数量
     */
    int modelCount() const;

    /**
     * @brief 获取模型根目录路径
     */
    QString modelsRootPath() const;

    // ========== Q_INVOKABLE 方法供 QML 调用 ==========

    /**
     * @brief 获取所有模型类别信息（QML 用）
     * @return 类别列表 [{id, name, icon, count}]
     */
    Q_INVOKABLE QVariantList getCategories() const;

    /**
     * @brief 按类别获取模型列表（QML 用）
     * @param categoryStr 类别字符串
     * @return 模型列表 [{id, name, description, ...}]
     */
    Q_INVOKABLE QVariantList getModelsByCategoryStr(const QString &categoryStr) const;

    /**
     * @brief 获取模型详情（QML 用）
     * @param modelId 模型 ID
     * @return 模型详情 Map
     */
    Q_INVOKABLE QVariantMap getModelInfoMap(const QString &modelId) const;

    /**
     * @brief 获取所有可用模型列表（QML 用）
     * @return 模型列表
     */
    Q_INVOKABLE QVariantList getAvailableModelsList() const;

signals:
    void modelsChanged();
    void modelRegistered(const QString &modelId);
    void registryError(const QString &error);

private:
    bool loadModelsJson(const QString &jsonPath);
    ModelCategory categoryFromString(const QString &str) const;
    QString categoryToString(ModelCategory category) const;
    QVariantMap modelInfoToVariantMap(const ModelInfo &info) const;

    static ModelRegistry* s_instance;
    QMap<QString, ModelInfo> m_models;
    QMap<QString, QVariantMap> m_categoryMeta;
    QString m_modelsRootPath;
};

} // namespace EnhanceVision

#endif // ENHANCEVISION_MODELREGISTRY_H
