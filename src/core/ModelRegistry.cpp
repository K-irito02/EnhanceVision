/**
 * @file ModelRegistry.cpp
 * @brief AI 模型注册与发现系统实现
 * @author Qt客户端开发工程师
 */

#include "EnhanceVision/core/ModelRegistry.h"
#include "EnhanceVision/controllers/SettingsController.h"
#include <QFile>
#include <QDir>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QFileInfo>
#include <QDebug>
#include <QLocale>

namespace EnhanceVision {

ModelRegistry* ModelRegistry::s_instance = nullptr;

ModelRegistry::ModelRegistry(QObject *parent)
    : QObject(parent)
{
}

ModelRegistry::~ModelRegistry()
{
}

ModelRegistry* ModelRegistry::instance()
{
    if (!s_instance) {
        s_instance = new ModelRegistry();
    }
    return s_instance;
}

int ModelRegistry::initialize(const QString &modelsRootPath)
{
    m_modelsRootPath = modelsRootPath;
    m_models.clear();
    m_categoryMeta.clear();

    QString jsonPath = modelsRootPath + "/models.json";
    if (!loadModelsJson(jsonPath)) {
        emit registryError(tr("无法加载模型配置文件: %1").arg(jsonPath));
        return 0;
    }

    int availableCount = 0;
    for (const auto &model : m_models) {
        if (model.isAvailable) {
            availableCount++;
        }
    }

    emit modelsChanged();
    return m_models.size();
}

bool ModelRegistry::loadModelsJson(const QString &jsonPath)
{
    QFile file(jsonPath);
    if (!file.open(QIODevice::ReadOnly)) {
        qWarning() << "[ModelRegistry] Cannot open:" << jsonPath;
        return false;
    }

    QJsonParseError parseError;
    QJsonDocument doc = QJsonDocument::fromJson(file.readAll(), &parseError);
    file.close();

    if (parseError.error != QJsonParseError::NoError) {
        qWarning() << "[ModelRegistry] JSON parse error:" << parseError.errorString();
        return false;
    }

    QJsonObject root = doc.object();

    // 加载类别元数据
    QJsonObject categories = root["categories"].toObject();
    for (auto it = categories.begin(); it != categories.end(); ++it) {
        QJsonObject catObj = it.value().toObject();
        QVariantMap catMeta;
        catMeta["id"] = it.key();
        catMeta["name"] = catObj["name"].toString();
        catMeta["name_en"] = catObj["name_en"].toString();
        catMeta["icon"] = catObj["icon"].toString();
        catMeta["order"] = catObj["order"].toInt();
        m_categoryMeta[it.key()] = catMeta;
    }

    // 加载模型列表
    QJsonArray modelsArray = root["models"].toArray();
    for (const QJsonValue &val : modelsArray) {
        QJsonObject obj = val.toObject();

        ModelInfo info;
        info.id = obj["id"].toString();
        info.name = obj["name"].toString();
        info.description = obj["description"].toString();
        info.descriptionEn = obj["description_en"].toString();
        info.category = categoryFromString(obj["category"].toString());
        info.scaleFactor = obj["scaleFactor"].toInt(1);
        info.inputChannels = obj["inputChannels"].toInt(3);
        info.outputChannels = obj["outputChannels"].toInt(3);
        info.tileSize = obj["tileSize"].toInt(0);
        info.tilePadding = obj["tilePadding"].toInt(10);
        info.inputBlobName = obj["inputBlob"].toString();
        info.outputBlobName = obj["outputBlob"].toString();

        // 归一化参数
        QJsonArray normMeanArr = obj["normMean"].toArray();
        QJsonArray normScaleArr = obj["normScale"].toArray();
        QJsonArray denormScaleArr = obj["denormScale"].toArray();
        QJsonArray denormMeanArr = obj["denormMean"].toArray();

        for (int i = 0; i < 3; ++i) {
            info.normMean[i] = normMeanArr.size() > i ? static_cast<float>(normMeanArr[i].toDouble()) : 0.f;
            info.normScale[i] = normScaleArr.size() > i ? static_cast<float>(normScaleArr[i].toDouble()) : 1/255.f;
            info.denormScale[i] = denormScaleArr.size() > i ? static_cast<float>(denormScaleArr[i].toDouble()) : 255.f;
            info.denormMean[i] = denormMeanArr.size() > i ? static_cast<float>(denormMeanArr[i].toDouble()) : 0.f;
        }

        // 支持的参数
        QJsonObject paramsObj = obj["supportedParams"].toObject();
        for (auto pit = paramsObj.begin(); pit != paramsObj.end(); ++pit) {
            info.supportedParams[pit.key()] = pit.value().toObject().toVariantMap();
        }

        // 解析文件路径并检查是否存在
        QString paramFile = obj["paramFile"].toString();
        QString binFile = obj["binFile"].toString();

        // 阶段性模型开关：当前仅启用 Real-ESRGAN 系列
        if (!isModelEnabledByPhase(info.id, paramFile)) {
            continue;
        }

        if (!paramFile.isEmpty() && !binFile.isEmpty()) {
            info.paramPath = m_modelsRootPath + "/" + paramFile;
            info.binPath = m_modelsRootPath + "/" + binFile;

            bool paramExists = QFile::exists(info.paramPath);
            bool binExists = QFile::exists(info.binPath);
            info.isAvailable = paramExists && binExists;

            if (info.isAvailable) {
                QFileInfo fi(info.binPath);
                info.sizeBytes = fi.size();
                QFileInfo fp(info.paramPath);
                info.sizeBytes += fp.size();
            } else {
                if (!paramExists) {
                    qDebug() << "[ModelRegistry] Missing param:" << info.paramPath;
                }
                if (!binExists) {
                    qDebug() << "[ModelRegistry] Missing bin:" << info.binPath;
                }
            }
        } else {
            // OpenCV 等无模型文件的方法，标记为可用
            QString engine = obj["engine"].toString();
            if (engine == "opencv") {
                info.isAvailable = true;
                info.paramPath.clear();
                info.binPath.clear();
            }
        }

        m_models[info.id] = info;
        emit modelRegistered(info.id);
    }

    return true;
}

ModelInfo ModelRegistry::getModelInfo(const QString &modelId) const
{
    return m_models.value(modelId);
}

QList<ModelInfo> ModelRegistry::getModelsByCategory(ModelCategory category) const
{
    QList<ModelInfo> result;
    for (const auto &model : m_models) {
        if (model.category == category) {
            result.append(model);
        }
    }
    return result;
}

QList<ModelInfo> ModelRegistry::getAvailableModels() const
{
    QList<ModelInfo> result;
    for (const auto &model : m_models) {
        if (model.isAvailable) {
            result.append(model);
        }
    }
    return result;
}

QList<ModelInfo> ModelRegistry::getAllModels() const
{
    return m_models.values();
}

bool ModelRegistry::hasModel(const QString &modelId) const
{
    return m_models.contains(modelId);
}

int ModelRegistry::modelCount() const
{
    return m_models.size();
}

QString ModelRegistry::modelsRootPath() const
{
    return m_modelsRootPath;
}

QVariantList ModelRegistry::getCategories() const
{
    QVariantList result;
    QList<QPair<int, QVariantMap>> sorted;

    // 从 SettingsController 获取当前语言设置
    QString currentLang = EnhanceVision::SettingsController::instance()->language();
    bool isEnglish = (currentLang == "en_US");

    for (auto it = m_categoryMeta.begin(); it != m_categoryMeta.end(); ++it) {
        int order = it.value()["order"].toInt();
        QVariantMap catInfo = it.value();

        // 统计该类别可用模型数
        ModelCategory cat = categoryFromString(it.key());
        int count = 0;
        for (const auto &model : m_models) {
            if (model.category == cat && model.isAvailable) {
                count++;
            }
        }
        catInfo["count"] = count;
        
        // 根据语言选择类别名称
        if (isEnglish && catInfo.contains("name_en")) {
            catInfo["name"] = catInfo["name_en"];
        }
        
        sorted.append(qMakePair(order, catInfo));
    }

    std::sort(sorted.begin(), sorted.end(),
              [](const QPair<int, QVariantMap> &a, const QPair<int, QVariantMap> &b) {
                  return a.first < b.first;
              });

    for (const auto &pair : sorted) {
        if (pair.second["count"].toInt() > 0) {
            result.append(pair.second);
        }
    }

    return result;
}

QVariantList ModelRegistry::getModelsByCategoryStr(const QString &categoryStr) const
{
    QVariantList result;
    ModelCategory cat = categoryFromString(categoryStr);
    for (const auto &model : m_models) {
        if (model.category == cat && model.isAvailable) {
            result.append(modelInfoToVariantMap(model));
        }
    }
    return result;
}

QVariantMap ModelRegistry::getModelInfoMap(const QString &modelId) const
{
    if (!m_models.contains(modelId)) {
        return {};
    }
    return modelInfoToVariantMap(m_models[modelId]);
}

QVariantList ModelRegistry::getAvailableModelsList() const
{
    QVariantList result;
    for (const auto &model : m_models) {
        if (model.isAvailable) {
            result.append(modelInfoToVariantMap(model));
        }
    }
    return result;
}

bool ModelRegistry::isModelEnabledByPhase(const QString &modelId, const QString &paramFilePath) const
{
    const QString idLower = modelId.toLower();
    const QString pathLower = paramFilePath.toLower();

    // 当前阶段：仅保留 Real-ESRGAN 系列模型
    // 允许 id/path 中包含以下关键词的模型通过：
    // - realesrgan-x4plus
    // - realesr-animevideov3
    const bool looksLikeRealEsrgan =
        idLower.contains("realesrgan") ||
        idLower.contains("realesr_animevideov3") ||
        pathLower.contains("real-esrgan") ||
        pathLower.contains("realesrgan") ||
        pathLower.contains("realesr-animevideov3");

    return looksLikeRealEsrgan;
}

ModelCategory ModelRegistry::categoryFromString(const QString &str) const
{
    static const QMap<QString, ModelCategory> map = {
        {"super_resolution", ModelCategory::SuperResolution},
        {"denoising", ModelCategory::Denoising},
        {"deblurring", ModelCategory::Deblurring},
        {"dehazing", ModelCategory::Dehazing},
        {"colorization", ModelCategory::Colorization},
        {"low_light", ModelCategory::LowLight},
        {"frame_interpolation", ModelCategory::FrameInterpolation},
        {"inpainting", ModelCategory::Inpainting}
    };
    return map.value(str, ModelCategory::SuperResolution);
}

QString ModelRegistry::categoryToString(ModelCategory category) const
{
    static const QMap<ModelCategory, QString> map = {
        {ModelCategory::SuperResolution, "super_resolution"},
        {ModelCategory::Denoising, "denoising"},
        {ModelCategory::Deblurring, "deblurring"},
        {ModelCategory::Dehazing, "dehazing"},
        {ModelCategory::Colorization, "colorization"},
        {ModelCategory::LowLight, "low_light"},
        {ModelCategory::FrameInterpolation, "frame_interpolation"},
        {ModelCategory::Inpainting, "inpainting"}
    };
    return map.value(category, "super_resolution");
}

QVariantMap ModelRegistry::modelInfoToVariantMap(const ModelInfo &info) const
{
    QVariantMap map;
    map["id"] = info.id;
    map["name"] = info.name;
    
    // 从 SettingsController 获取当前语言设置
    QString currentLang = EnhanceVision::SettingsController::instance()->language();
    bool isEnglish = (currentLang == "en_US");
    
    if (isEnglish && !info.descriptionEn.isEmpty()) {
        map["description"] = info.descriptionEn;
    } else {
        map["description"] = info.description;
    }
    
    map["category"] = categoryToString(info.category);
    map["scaleFactor"] = info.scaleFactor;
    map["sizeBytes"] = info.sizeBytes;
    map["isAvailable"] = info.isAvailable;
    map["isLoaded"] = info.isLoaded;
    map["tileSize"] = info.tileSize;
    map["supportedParams"] = info.supportedParams;

    // 可读的大小字符串
    if (info.sizeBytes > 1024 * 1024) {
        map["sizeStr"] = QString("%1 MB").arg(info.sizeBytes / (1024.0 * 1024.0), 0, 'f', 1);
    } else if (info.sizeBytes > 1024) {
        map["sizeStr"] = QString("%1 KB").arg(info.sizeBytes / 1024.0, 0, 'f', 1);
    } else if (info.sizeBytes > 0) {
        map["sizeStr"] = QString("%1 B").arg(info.sizeBytes);
    } else {
        map["sizeStr"] = isEnglish ? "Traditional Algorithm" : "传统算法";
    }

    // 类别显示名（根据语言选择）
    QString catStr = categoryToString(info.category);
    if (m_categoryMeta.contains(catStr)) {
        if (isEnglish && m_categoryMeta[catStr].contains("name_en")) {
            map["categoryName"] = m_categoryMeta[catStr]["name_en"];
        } else {
            map["categoryName"] = m_categoryMeta[catStr]["name"];
        }
        map["categoryIcon"] = m_categoryMeta[catStr]["icon"];
    }

    return map;
}

} // namespace EnhanceVision
