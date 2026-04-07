/**
 * @file UIStateController.h
 * @brief UI状态控制器 - 统一管理所有UI界面状态的持久化
 * @author Qt客户端开发工程师
 */

#ifndef ENHANCEVISION_UISTATECONTROLLER_H
#define ENHANCEVISION_UISTATECONTROLLER_H

#include <QObject>
#include <QJsonObject>
#include <QJsonArray>
#include <QString>
#include <QVariantMap>

namespace EnhanceVision {

/**
 * @brief UI状态控制器 - 单例模式
 * 
 * 统一管理所有UI界面状态的持久化，包括：
 * - 侧边栏状态（展开/收缩、宽度）
 * - 控制面板状态（收缩、宽度、处理模式）
 * - Shader模式参数（14个参数 + 预设）
 * - AI模式参数（模型选择、CPU/GPU、分块大小等）
 */
class UIStateController : public QObject
{
    Q_OBJECT

    // ========== 侧边栏状态 ==========
    Q_PROPERTY(bool sidebarExpanded READ sidebarExpanded WRITE setSidebarExpanded NOTIFY sidebarExpandedChanged)
    Q_PROPERTY(int sidebarWidth READ sidebarWidth WRITE setSidebarWidth NOTIFY sidebarWidthChanged)

    // ========== 控制面板状态 ==========
    Q_PROPERTY(bool controlPanelCollapsed READ controlPanelCollapsed WRITE setControlPanelCollapsed NOTIFY controlPanelCollapsedChanged)
    Q_PROPERTY(int controlPanelWidth READ controlPanelWidth WRITE setControlPanelWidth NOTIFY controlPanelWidthChanged)
    Q_PROPERTY(int processingMode READ processingMode WRITE setProcessingMode NOTIFY processingModeChanged)

    // ========== Shader模式参数 ==========
    Q_PROPERTY(double brightness READ brightness WRITE setBrightness NOTIFY shaderParamsChanged)
    Q_PROPERTY(double contrast READ contrast WRITE setContrast NOTIFY shaderParamsChanged)
    Q_PROPERTY(double saturation READ saturation WRITE setSaturation NOTIFY shaderParamsChanged)
    Q_PROPERTY(double hue READ hue WRITE setHue NOTIFY shaderParamsChanged)
    Q_PROPERTY(double sharpness READ sharpness WRITE setSharpness NOTIFY shaderParamsChanged)
    Q_PROPERTY(double blur READ blur WRITE setBlur NOTIFY shaderParamsChanged)
    Q_PROPERTY(double denoise READ denoise WRITE setDenoise NOTIFY shaderParamsChanged)
    Q_PROPERTY(double exposure READ exposure WRITE setExposure NOTIFY shaderParamsChanged)
    Q_PROPERTY(double gamma READ gamma WRITE setGamma NOTIFY shaderParamsChanged)
    Q_PROPERTY(double temperature READ temperature WRITE setTemperature NOTIFY shaderParamsChanged)
    Q_PROPERTY(double tint READ tint WRITE setTint NOTIFY shaderParamsChanged)
    Q_PROPERTY(double vignette READ vignette WRITE setVignette NOTIFY shaderParamsChanged)
    Q_PROPERTY(double highlights READ highlights WRITE setHighlights NOTIFY shaderParamsChanged)
    Q_PROPERTY(double shadows READ shadows WRITE setShadows NOTIFY shaderParamsChanged)
    Q_PROPERTY(QString shaderSelectedCategory READ shaderSelectedCategory WRITE setShaderSelectedCategory NOTIFY shaderSelectedCategoryChanged)

    // ========== AI模式参数 ==========
    Q_PROPERTY(QString aiSelectedModelId READ aiSelectedModelId WRITE setAISelectedModelId NOTIFY aiSelectedModelIdChanged)
    Q_PROPERTY(QString aiSelectedCategory READ aiSelectedCategory WRITE setAISelectedCategory NOTIFY aiSelectedCategoryChanged)
    Q_PROPERTY(bool aiUseGpu READ aiUseGpu WRITE setAIUseGpu NOTIFY aiUseGpuChanged)
    Q_PROPERTY(int aiTileSize READ aiTileSize WRITE setAITileSize NOTIFY aiTileSizeChanged)
    Q_PROPERTY(bool aiAutoTileMode READ aiAutoTileMode WRITE setAIAutoTileMode NOTIFY aiAutoTileModeChanged)

public:
    static UIStateController* instance();
    static void destroyInstance();

    // ========== 侧边栏状态 ==========
    bool sidebarExpanded() const;
    void setSidebarExpanded(bool expanded);

    int sidebarWidth() const;
    void setSidebarWidth(int width);

    // ========== 控制面板状态 ==========
    bool controlPanelCollapsed() const;
    void setControlPanelCollapsed(bool collapsed);

    int controlPanelWidth() const;
    void setControlPanelWidth(int width);

    int processingMode() const;
    void setProcessingMode(int mode);

    // ========== Shader模式参数 ==========
    double brightness() const;
    void setBrightness(double value);

    double contrast() const;
    void setContrast(double value);

    double saturation() const;
    void setSaturation(double value);

    double hue() const;
    void setHue(double value);

    double sharpness() const;
    void setSharpness(double value);

    double blur() const;
    void setBlur(double value);

    double denoise() const;
    void setDenoise(double value);

    double exposure() const;
    void setExposure(double value);

    double gamma() const;
    void setGamma(double value);

    double temperature() const;
    void setTemperature(double value);

    double tint() const;
    void setTint(double value);

    double vignette() const;
    void setVignette(double value);

    double highlights() const;
    void setHighlights(double value);

    double shadows() const;
    void setShadows(double value);

    QString shaderSelectedCategory() const;
    void setShaderSelectedCategory(const QString& category);

    // ========== AI模式参数 ==========
    QString aiSelectedModelId() const;
    void setAISelectedModelId(const QString& modelId);

    QString aiSelectedCategory() const;
    void setAISelectedCategory(const QString& category);

    bool aiUseGpu() const;
    void setAIUseGpu(bool useGpu);

    int aiTileSize() const;
    void setAITileSize(int size);

    bool aiAutoTileMode() const;
    void setAIAutoTileMode(bool autoMode);

    // ========== 自定义预设和类别 ==========
    Q_INVOKABLE QJsonArray getCustomCategories() const;
    Q_INVOKABLE void setCustomCategories(const QJsonArray& categories);

    Q_INVOKABLE QJsonArray getCustomPresets() const;
    Q_INVOKABLE void setCustomPresets(const QJsonArray& presets);

    // ========== AI模型参数 ==========
    Q_INVOKABLE QVariantMap getAIModelParams() const;
    Q_INVOKABLE void setAIModelParams(const QVariantMap& params);

    // ========== 保存和加载 ==========
    Q_INVOKABLE void saveState();
    Q_INVOKABLE void loadState();

signals:
    void sidebarExpandedChanged();
    void sidebarWidthChanged();
    void controlPanelCollapsedChanged();
    void controlPanelWidthChanged();
    void processingModeChanged();
    void shaderParamsChanged();
    void shaderSelectedCategoryChanged();
    void aiSelectedModelIdChanged();
    void aiSelectedCategoryChanged();
    void aiUseGpuChanged();
    void aiTileSizeChanged();
    void aiAutoTileModeChanged();
    void customCategoriesChanged();
    void customPresetsChanged();
    void aiModelParamsChanged();

private:
    explicit UIStateController(QObject* parent = nullptr);
    ~UIStateController() override;

    UIStateController(const UIStateController&) = delete;
    UIStateController& operator=(const UIStateController&) = delete;

    QString getStateFilePath() const;
    void ensureStateDirectoryExists();

    static UIStateController* s_instance;

    // ========== 侧边栏状态 ==========
    bool m_sidebarExpanded;
    int m_sidebarWidth;

    // ========== 控制面板状态 ==========
    bool m_controlPanelCollapsed;
    int m_controlPanelWidth;
    int m_processingMode;

    // ========== Shader模式参数 ==========
    double m_brightness;
    double m_contrast;
    double m_saturation;
    double m_hue;
    double m_sharpness;
    double m_blur;
    double m_denoise;
    double m_exposure;
    double m_gamma;
    double m_temperature;
    double m_tint;
    double m_vignette;
    double m_highlights;
    double m_shadows;
    QString m_shaderSelectedCategory;

    // ========== AI模式参数 ==========
    QString m_aiSelectedModelId;
    QString m_aiSelectedCategory;
    bool m_aiUseGpu;
    int m_aiTileSize;
    bool m_aiAutoTileMode;
    QVariantMap m_aiModelParams;

    // ========== 自定义预设和类别 ==========
    QJsonArray m_customCategories;
    QJsonArray m_customPresets;
};

} // namespace EnhanceVision

#endif // ENHANCEVISION_UISTATECONTROLLER_H
