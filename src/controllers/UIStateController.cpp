/**
 * @file UIStateController.cpp
 * @brief UI状态控制器实现
 * @author Qt客户端开发工程师
 */

#include "EnhanceVision/controllers/UIStateController.h"
#include <QStandardPaths>
#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QJsonValue>
#include <QDebug>

namespace EnhanceVision {

UIStateController* UIStateController::s_instance = nullptr;

UIStateController::UIStateController(QObject* parent)
    : QObject(parent)
    , m_sidebarExpanded(true)
    , m_sidebarWidth(240)
    , m_controlPanelCollapsed(false)
    , m_controlPanelWidth(320)
    , m_processingMode(0)
    , m_brightness(0.0)
    , m_contrast(1.0)
    , m_saturation(1.0)
    , m_hue(0.0)
    , m_sharpness(0.0)
    , m_blur(0.0)
    , m_denoise(0.0)
    , m_exposure(0.0)
    , m_gamma(1.0)
    , m_temperature(0.0)
    , m_tint(0.0)
    , m_vignette(0.0)
    , m_highlights(0.0)
    , m_shadows(0.0)
    , m_shaderSelectedCategory("all")
    , m_aiSelectedModelId("")
    , m_aiSelectedCategory("")
    , m_aiUseGpu(false)
    , m_aiTileSize(0)
    , m_aiAutoTileMode(true)
{
    loadState();
}

UIStateController::~UIStateController()
{
    saveState();
}

UIStateController* UIStateController::instance()
{
    if (!s_instance) {
        s_instance = new UIStateController();
    }
    return s_instance;
}

void UIStateController::destroyInstance()
{
    if (s_instance) {
        delete s_instance;
        s_instance = nullptr;
    }
}

QString UIStateController::getStateFilePath() const
{
    QString configPath = QStandardPaths::writableLocation(QStandardPaths::ConfigLocation);
    return QDir(configPath).filePath("ui_state.json");
}

void UIStateController::ensureStateDirectoryExists()
{
    QString configPath = QStandardPaths::writableLocation(QStandardPaths::ConfigLocation);
    QDir dir(configPath);
    if (!dir.exists()) {
        dir.mkpath(".");
    }
}

// ========== 侧边栏状态 ==========

bool UIStateController::sidebarExpanded() const
{
    return m_sidebarExpanded;
}

void UIStateController::setSidebarExpanded(bool expanded)
{
    if (m_sidebarExpanded != expanded) {
        m_sidebarExpanded = expanded;
        emit sidebarExpandedChanged();
        saveState();
    }
}

int UIStateController::sidebarWidth() const
{
    return m_sidebarWidth;
}

void UIStateController::setSidebarWidth(int width)
{
    if (m_sidebarWidth != width) {
        m_sidebarWidth = width;
        emit sidebarWidthChanged();
        saveState();
    }
}

// ========== 控制面板状态 ==========

bool UIStateController::controlPanelCollapsed() const
{
    return m_controlPanelCollapsed;
}

void UIStateController::setControlPanelCollapsed(bool collapsed)
{
    if (m_controlPanelCollapsed != collapsed) {
        m_controlPanelCollapsed = collapsed;
        emit controlPanelCollapsedChanged();
        saveState();
    }
}

int UIStateController::controlPanelWidth() const
{
    return m_controlPanelWidth;
}

void UIStateController::setControlPanelWidth(int width)
{
    if (m_controlPanelWidth != width) {
        m_controlPanelWidth = width;
        emit controlPanelWidthChanged();
        saveState();
    }
}

int UIStateController::processingMode() const
{
    return m_processingMode;
}

void UIStateController::setProcessingMode(int mode)
{
    if (m_processingMode != mode) {
        m_processingMode = mode;
        emit processingModeChanged();
        saveState();
    }
}

// ========== Shader模式参数 ==========

double UIStateController::brightness() const { return m_brightness; }
void UIStateController::setBrightness(double value) { if (m_brightness != value) { m_brightness = value; emit shaderParamsChanged(); saveState(); } }

double UIStateController::contrast() const { return m_contrast; }
void UIStateController::setContrast(double value) { if (m_contrast != value) { m_contrast = value; emit shaderParamsChanged(); saveState(); } }

double UIStateController::saturation() const { return m_saturation; }
void UIStateController::setSaturation(double value) { if (m_saturation != value) { m_saturation = value; emit shaderParamsChanged(); saveState(); } }

double UIStateController::hue() const { return m_hue; }
void UIStateController::setHue(double value) { if (m_hue != value) { m_hue = value; emit shaderParamsChanged(); saveState(); } }

double UIStateController::sharpness() const { return m_sharpness; }
void UIStateController::setSharpness(double value) { if (m_sharpness != value) { m_sharpness = value; emit shaderParamsChanged(); saveState(); } }

double UIStateController::blur() const { return m_blur; }
void UIStateController::setBlur(double value) { if (m_blur != value) { m_blur = value; emit shaderParamsChanged(); saveState(); } }

double UIStateController::denoise() const { return m_denoise; }
void UIStateController::setDenoise(double value) { if (m_denoise != value) { m_denoise = value; emit shaderParamsChanged(); saveState(); } }

double UIStateController::exposure() const { return m_exposure; }
void UIStateController::setExposure(double value) { if (m_exposure != value) { m_exposure = value; emit shaderParamsChanged(); saveState(); } }

double UIStateController::gamma() const { return m_gamma; }
void UIStateController::setGamma(double value) { if (m_gamma != value) { m_gamma = value; emit shaderParamsChanged(); saveState(); } }

double UIStateController::temperature() const { return m_temperature; }
void UIStateController::setTemperature(double value) { if (m_temperature != value) { m_temperature = value; emit shaderParamsChanged(); saveState(); } }

double UIStateController::tint() const { return m_tint; }
void UIStateController::setTint(double value) { if (m_tint != value) { m_tint = value; emit shaderParamsChanged(); saveState(); } }

double UIStateController::vignette() const { return m_vignette; }
void UIStateController::setVignette(double value) { if (m_vignette != value) { m_vignette = value; emit shaderParamsChanged(); saveState(); } }

double UIStateController::highlights() const { return m_highlights; }
void UIStateController::setHighlights(double value) { if (m_highlights != value) { m_highlights = value; emit shaderParamsChanged(); saveState(); } }

double UIStateController::shadows() const { return m_shadows; }
void UIStateController::setShadows(double value) { if (m_shadows != value) { m_shadows = value; emit shaderParamsChanged(); saveState(); } }

QString UIStateController::shaderSelectedCategory() const { return m_shaderSelectedCategory; }
void UIStateController::setShaderSelectedCategory(const QString& category) 
{ 
    if (m_shaderSelectedCategory != category) { 
        m_shaderSelectedCategory = category; 
        emit shaderSelectedCategoryChanged(); 
        saveState(); 
    } 
}

// ========== AI模式参数 ==========

QString UIStateController::aiSelectedModelId() const { return m_aiSelectedModelId; }
void UIStateController::setAISelectedModelId(const QString& modelId) 
{ 
    if (m_aiSelectedModelId != modelId) { 
        m_aiSelectedModelId = modelId; 
        emit aiSelectedModelIdChanged(); 
        saveState(); 
    } 
}

QString UIStateController::aiSelectedCategory() const { return m_aiSelectedCategory; }
void UIStateController::setAISelectedCategory(const QString& category) 
{ 
    if (m_aiSelectedCategory != category) { 
        m_aiSelectedCategory = category; 
        emit aiSelectedCategoryChanged(); 
        saveState(); 
    } 
}

bool UIStateController::aiUseGpu() const { return m_aiUseGpu; }
void UIStateController::setAIUseGpu(bool useGpu) 
{ 
    if (m_aiUseGpu != useGpu) { 
        m_aiUseGpu = useGpu; 
        emit aiUseGpuChanged(); 
        saveState(); 
    } 
}

int UIStateController::aiTileSize() const { return m_aiTileSize; }
void UIStateController::setAITileSize(int size) 
{ 
    if (m_aiTileSize != size) { 
        m_aiTileSize = size; 
        emit aiTileSizeChanged(); 
        saveState(); 
    } 
}

bool UIStateController::aiAutoTileMode() const { return m_aiAutoTileMode; }
void UIStateController::setAIAutoTileMode(bool autoMode) 
{ 
    if (m_aiAutoTileMode != autoMode) { 
        m_aiAutoTileMode = autoMode; 
        emit aiAutoTileModeChanged(); 
        saveState(); 
    } 
}

bool UIStateController::hasWindowLayout(const QString& key) const
{
    const auto it = m_windowLayouts.constFind(key);
    return it != m_windowLayouts.constEnd() && it->isValid();
}

UIStateController::WindowLayout UIStateController::windowLayout(const QString& key) const
{
    const auto it = m_windowLayouts.constFind(key);
    if (it == m_windowLayouts.constEnd()) {
        return {};
    }
    return it.value();
}

void UIStateController::setWindowLayout(const QString& key, const QRect& normalGeometry, bool maximized)
{
    if (key.isEmpty() || !normalGeometry.isValid() || normalGeometry.width() <= 0 || normalGeometry.height() <= 0) {
        return;
    }

    WindowLayout nextLayout;
    nextLayout.normalGeometry = normalGeometry;
    nextLayout.maximized = maximized;

    const auto current = m_windowLayouts.value(key);
    if (current.normalGeometry == nextLayout.normalGeometry && current.maximized == nextLayout.maximized) {
        return;
    }

    m_windowLayouts.insert(key, nextLayout);
    emit windowLayoutChanged(key);
    saveState();
}

// ========== 自定义预设和类别 ==========

QJsonArray UIStateController::getCustomCategories() const
{
    return m_customCategories;
}

void UIStateController::setCustomCategories(const QJsonArray& categories)
{
    m_customCategories = categories;
    emit customCategoriesChanged();
    saveState();
}

QJsonArray UIStateController::getCustomPresets() const
{
    return m_customPresets;
}

void UIStateController::setCustomPresets(const QJsonArray& presets)
{
    m_customPresets = presets;
    emit customPresetsChanged();
    saveState();
}

// ========== AI模型参数 ==========

QVariantMap UIStateController::getAIModelParams() const
{
    return m_aiModelParams;
}

void UIStateController::setAIModelParams(const QVariantMap& params)
{
    m_aiModelParams = params;
    emit aiModelParamsChanged();
    saveState();
}

// ========== 保存和加载 ==========

void UIStateController::saveState()
{
    ensureStateDirectoryExists();
    
    QJsonObject root;
    
    // 侧边栏状态
    QJsonObject sidebar;
    sidebar["expanded"] = m_sidebarExpanded;
    sidebar["width"] = m_sidebarWidth;
    root["sidebar"] = sidebar;
    
    // 控制面板状态
    QJsonObject controlPanel;
    controlPanel["collapsed"] = m_controlPanelCollapsed;
    controlPanel["width"] = m_controlPanelWidth;
    controlPanel["processingMode"] = m_processingMode;
    root["controlPanel"] = controlPanel;
    
    // Shader模式参数
    QJsonObject shaderParams;
    shaderParams["brightness"] = m_brightness;
    shaderParams["contrast"] = m_contrast;
    shaderParams["saturation"] = m_saturation;
    shaderParams["hue"] = m_hue;
    shaderParams["sharpness"] = m_sharpness;
    shaderParams["blur"] = m_blur;
    shaderParams["denoise"] = m_denoise;
    shaderParams["exposure"] = m_exposure;
    shaderParams["gamma"] = m_gamma;
    shaderParams["temperature"] = m_temperature;
    shaderParams["tint"] = m_tint;
    shaderParams["vignette"] = m_vignette;
    shaderParams["highlights"] = m_highlights;
    shaderParams["shadows"] = m_shadows;
    shaderParams["selectedCategory"] = m_shaderSelectedCategory;
    shaderParams["customCategories"] = m_customCategories;
    shaderParams["customPresets"] = m_customPresets;
    root["shaderParams"] = shaderParams;
    
    // AI模式参数
    QJsonObject aiParams;
    aiParams["selectedModelId"] = m_aiSelectedModelId;
    aiParams["selectedCategory"] = m_aiSelectedCategory;
    aiParams["useGpu"] = m_aiUseGpu;
    aiParams["tileSize"] = m_aiTileSize;
    aiParams["autoTileMode"] = m_aiAutoTileMode;
    aiParams["modelParams"] = QJsonObject::fromVariantMap(m_aiModelParams);
    root["aiParams"] = aiParams;

    saveWindowLayouts(root);
    
    // 写入文件
    QString filePath = getStateFilePath();
    QFile file(filePath);
    if (file.open(QIODevice::WriteOnly)) {
        QJsonDocument doc(root);
        file.write(doc.toJson(QJsonDocument::Indented));
        file.close();
        qInfo() << "[UIStateController] State saved to:" << filePath;
    } else {
        qWarning() << "[UIStateController] Failed to save state to:" << filePath;
    }
}

void UIStateController::loadState()
{
    QString filePath = getStateFilePath();
    QFile file(filePath);
    
    if (!file.exists()) {
        qInfo() << "[UIStateController] State file not found, using defaults:" << filePath;
        return;
    }
    
    if (!file.open(QIODevice::ReadOnly)) {
        qWarning() << "[UIStateController] Failed to open state file:" << filePath;
        return;
    }
    
    QByteArray data = file.readAll();
    file.close();
    
    QJsonParseError parseError;
    QJsonDocument doc = QJsonDocument::fromJson(data, &parseError);
    
    if (parseError.error != QJsonParseError::NoError) {
        qWarning() << "[UIStateController] Failed to parse state file:" << parseError.errorString();
        return;
    }
    
    QJsonObject root = doc.object();
    
    // 加载侧边栏状态
    if (root.contains("sidebar")) {
        QJsonObject sidebar = root["sidebar"].toObject();
        m_sidebarExpanded = sidebar["expanded"].toBool(true);
        m_sidebarWidth = sidebar["width"].toInt(240);
    }
    
    // 加载控制面板状态
    if (root.contains("controlPanel")) {
        QJsonObject controlPanel = root["controlPanel"].toObject();
        m_controlPanelCollapsed = controlPanel["collapsed"].toBool(false);
        m_controlPanelWidth = controlPanel["width"].toInt(320);
        m_processingMode = controlPanel["processingMode"].toInt(0);
    }
    
    // 加载Shader模式参数
    if (root.contains("shaderParams")) {
        QJsonObject shaderParams = root["shaderParams"].toObject();
        m_brightness = shaderParams["brightness"].toDouble(0.0);
        m_contrast = shaderParams["contrast"].toDouble(1.0);
        m_saturation = shaderParams["saturation"].toDouble(1.0);
        m_hue = shaderParams["hue"].toDouble(0.0);
        m_sharpness = shaderParams["sharpness"].toDouble(0.0);
        m_blur = shaderParams["blur"].toDouble(0.0);
        m_denoise = shaderParams["denoise"].toDouble(0.0);
        m_exposure = shaderParams["exposure"].toDouble(0.0);
        m_gamma = shaderParams["gamma"].toDouble(1.0);
        m_temperature = shaderParams["temperature"].toDouble(0.0);
        m_tint = shaderParams["tint"].toDouble(0.0);
        m_vignette = shaderParams["vignette"].toDouble(0.0);
        m_highlights = shaderParams["highlights"].toDouble(0.0);
        m_shadows = shaderParams["shadows"].toDouble(0.0);
        m_shaderSelectedCategory = shaderParams["selectedCategory"].toString("all");
        m_customCategories = shaderParams["customCategories"].toArray();
        m_customPresets = shaderParams["customPresets"].toArray();
    }
    
    // 加载AI模式参数
    if (root.contains("aiParams")) {
        QJsonObject aiParams = root["aiParams"].toObject();
        m_aiSelectedModelId = aiParams["selectedModelId"].toString();
        m_aiSelectedCategory = aiParams["selectedCategory"].toString();
        m_aiUseGpu = aiParams["useGpu"].toBool(false);
        m_aiTileSize = aiParams["tileSize"].toInt(0);
        m_aiAutoTileMode = aiParams["autoTileMode"].toBool(true);
        m_aiModelParams = aiParams["modelParams"].toObject().toVariantMap();
    }

    loadWindowLayouts(root);
    
    qInfo() << "[UIStateController] State loaded from:" << filePath;
}

void UIStateController::loadWindowLayouts(const QJsonObject& root)
{
    m_windowLayouts.clear();

    const QJsonObject windows = root.value("windows").toObject();
    for (auto it = windows.begin(); it != windows.end(); ++it) {
        const QJsonObject layoutObject = it->toObject();
        const QJsonObject geometryObject = layoutObject.value("normalGeometry").toObject();

        const QRect geometry(
            geometryObject.value("x").toInt(),
            geometryObject.value("y").toInt(),
            geometryObject.value("width").toInt(),
            geometryObject.value("height").toInt());

        WindowLayout layout;
        layout.normalGeometry = geometry;
        layout.maximized = layoutObject.value("maximized").toBool(false);

        if (layout.isValid()) {
            m_windowLayouts.insert(it.key(), layout);
        }
    }
}

void UIStateController::saveWindowLayouts(QJsonObject& root) const
{
    QJsonObject windows;
    for (auto it = m_windowLayouts.constBegin(); it != m_windowLayouts.constEnd(); ++it) {
        if (!it->isValid()) {
            continue;
        }

        QJsonObject geometryObject;
        geometryObject["x"] = it->normalGeometry.x();
        geometryObject["y"] = it->normalGeometry.y();
        geometryObject["width"] = it->normalGeometry.width();
        geometryObject["height"] = it->normalGeometry.height();

        QJsonObject layoutObject;
        layoutObject["normalGeometry"] = geometryObject;
        layoutObject["maximized"] = it->maximized;
        windows[it.key()] = layoutObject;
    }

    root["windows"] = windows;
}

} // namespace EnhanceVision
