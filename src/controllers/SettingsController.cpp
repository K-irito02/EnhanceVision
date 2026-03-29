/**
 * @file SettingsController.cpp
 * @brief 设置控制器实现
 * @author Qt客户端开发工程师
 */

#include "EnhanceVision/controllers/SettingsController.h"
#include "EnhanceVision/core/ResourceManager.h"
#include <QStandardPaths>
#include <QDir>
#include <QStringList>

#ifdef Q_OS_WIN
#include <windows.h>
#include <dxgi.h>
#pragma comment(lib, "dxgi.lib")
#endif

namespace EnhanceVision {

SettingsController* SettingsController::s_instance = nullptr;

SettingsController::SettingsController(QObject* parent)
    : QObject(parent)
    , m_settings(nullptr)
    , m_theme("dark")
    , m_language("zh_CN")
    , m_sidebarExpanded(true)
    , m_autoSaveResult(false)
    , m_volume(80)
    , m_autoReprocessShaderEnabled(true)
    , m_autoReprocessAIEnabled(true)
    , m_lastExitClean(true)
    , m_crashDetectedOnStartup(false)
    , m_lastExitReason()
    , m_videoAutoPlay(true)
{
    QString configPath = QStandardPaths::writableLocation(QStandardPaths::ConfigLocation);
    QString settingsFile = QDir(configPath).filePath("EnhanceVision/settings.ini");
    m_settings = new QSettings(settingsFile, QSettings::IniFormat, this);

    // 设置默认保存路径
    QString picturesPath = QStandardPaths::writableLocation(QStandardPaths::PicturesLocation);
    m_defaultSavePath = QDir(picturesPath).filePath("EnhanceVision");

    loadSettings();
}

SettingsController::~SettingsController()
{
    saveSettings();
}

SettingsController* SettingsController::instance()
{
    if (!s_instance) {
        s_instance = new SettingsController();
    }
    return s_instance;
}

void SettingsController::destroyInstance()
{
    if (s_instance) {
        delete s_instance;
        s_instance = nullptr;
    }
}

QString SettingsController::theme() const
{
    return m_theme;
}

void SettingsController::setTheme(const QString& theme)
{
    if (m_theme != theme) {
        m_theme = theme;
        emit themeChanged();
        emit settingsChanged();
        saveSettings();
    }
}

QString SettingsController::language() const
{
    return m_language;
}

void SettingsController::setLanguage(const QString& language)
{
    if (m_language != language) {
        m_language = language;
        emit languageChanged();
        emit settingsChanged();
        saveSettings();
    }
}

bool SettingsController::sidebarExpanded() const
{
    return m_sidebarExpanded;
}

void SettingsController::setSidebarExpanded(bool expanded)
{
    if (m_sidebarExpanded != expanded) {
        m_sidebarExpanded = expanded;
        emit sidebarExpandedChanged();
        emit settingsChanged();
    }
}

QString SettingsController::defaultSavePath() const
{
    return m_defaultSavePath;
}

void SettingsController::setDefaultSavePath(const QString& path)
{
    if (m_defaultSavePath != path) {
        m_defaultSavePath = path;
        emit defaultSavePathChanged();
        emit settingsChanged();
    }
}

bool SettingsController::autoSaveResult() const
{
    return m_autoSaveResult;
}

void SettingsController::setAutoSaveResult(bool autoSave)
{
    if (m_autoSaveResult != autoSave) {
        m_autoSaveResult = autoSave;
        emit autoSaveResultChanged();
        emit settingsChanged();
    }
}

int SettingsController::volume() const
{
    return m_volume;
}

void SettingsController::setVolume(int volume)
{
    int clampedVolume = qBound(0, volume, 100);
    if (m_volume != clampedVolume) {
        m_volume = clampedVolume;
        emit volumeChanged();
        emit settingsChanged();
        saveSettings();
    }
}

bool SettingsController::autoReprocessShaderEnabled() const
{
    return m_autoReprocessShaderEnabled;
}

void SettingsController::setAutoReprocessShaderEnabled(bool enabled)
{
    if (m_autoReprocessShaderEnabled != enabled) {
        m_autoReprocessShaderEnabled = enabled;
        emit autoReprocessShaderEnabledChanged();
        emit autoReprocessAllEnabledChanged();
        emit settingsChanged();
        saveSettings();
    }
}

bool SettingsController::autoReprocessAIEnabled() const
{
    return m_autoReprocessAIEnabled;
}

void SettingsController::setAutoReprocessAIEnabled(bool enabled)
{
    if (m_autoReprocessAIEnabled != enabled) {
        m_autoReprocessAIEnabled = enabled;
        emit autoReprocessAIEnabledChanged();
        emit autoReprocessAllEnabledChanged();
        emit settingsChanged();
        saveSettings();
    }
}

bool SettingsController::autoReprocessAllEnabled() const
{
    return m_autoReprocessShaderEnabled && m_autoReprocessAIEnabled;
}

void SettingsController::setAutoReprocessAllEnabled(bool enabled)
{
    if (autoReprocessAllEnabled() != enabled) {
        m_autoReprocessShaderEnabled = enabled;
        m_autoReprocessAIEnabled = enabled;
        emit autoReprocessShaderEnabledChanged();
        emit autoReprocessAIEnabledChanged();
        emit autoReprocessAllEnabledChanged();
        emit settingsChanged();
        saveSettings();
    }
}

bool SettingsController::videoAutoPlay() const
{
    return m_videoAutoPlay;
}

void SettingsController::setVideoAutoPlay(bool autoPlay)
{
    if (m_videoAutoPlay != autoPlay) {
        m_videoAutoPlay = autoPlay;
        emit videoAutoPlayChanged();
        emit settingsChanged();
        saveSettings();
    }
}

bool SettingsController::lastExitClean() const
{
    return m_lastExitClean;
}

bool SettingsController::crashDetectedOnStartup() const
{
    return m_crashDetectedOnStartup;
}

QString SettingsController::lastExitReason() const
{
    return m_lastExitReason;
}

void SettingsController::markAppRunning()
{
    m_lastExitClean = false;
    m_lastExitReason.clear();
    m_settings->setValue("system/lastExitClean", false);
    m_settings->remove("system/lastExitReason");
    m_settings->sync();
}

void SettingsController::markAppExiting()
{
    m_lastExitClean = true;
    m_settings->setValue("system/lastExitClean", true);
    m_settings->sync();
}

void SettingsController::markNormalExit(const QString& reason)
{
    m_lastExitClean = true;
    m_lastExitReason = reason;
    m_settings->setValue("system/lastExitClean", true);
    m_settings->setValue("system/lastExitReason", reason);
    m_settings->sync();
    qInfo() << "[SettingsController] Normal exit marked with reason:" << reason;
}

bool SettingsController::checkAndHandleCrashRecovery()
{
    bool wasClean = m_settings->value("system/lastExitClean", true).toBool();
    QString lastReason = m_settings->value("system/lastExitReason", QString()).toString();
    
    qInfo() << "[SettingsController] Checking crash recovery: lastExitClean=" << wasClean 
            << ", lastExitReason=" << lastReason;
    
    if (wasClean) {
        qInfo() << "[SettingsController] Last exit was clean, no crash recovery needed";
        return false;
    }
    
    QStringList normalReasons = {
        QStringLiteral("normal"),
        QStringLiteral("main_window_closed"),
        QStringLiteral("user_request")
    };
    
    if (!lastReason.isEmpty() && normalReasons.contains(lastReason)) {
        qInfo() << "[SettingsController] Exit reason recorded as normal:" << lastReason 
                << ", treating as normal exit";
        return false;
    }
    
    qWarning() << "[SettingsController] Abnormal exit detected! lastExitClean=" << wasClean
               << ", lastExitReason=" << lastReason;
    
    m_crashDetectedOnStartup = true;
    m_autoReprocessShaderEnabled = false;
    m_autoReprocessAIEnabled = false;
    
    m_settings->setValue("reprocess/shaderEnabled", false);
    m_settings->setValue("reprocess/aiEnabled", false);
    m_settings->sync();
    
    emit autoReprocessShaderEnabledChanged();
    emit autoReprocessAIEnabledChanged();
    emit autoReprocessAllEnabledChanged();
    emit crashDetected();
    emit crashDetectedOnStartupChanged();
    
    return true;
}

void SettingsController::saveSettings()
{
    if (!m_settings) return;

    m_settings->setValue("appearance/theme", m_theme);
    m_settings->setValue("appearance/language", m_language);
    m_settings->setValue("sidebar/expanded", m_sidebarExpanded);
    m_settings->setValue("behavior/defaultSavePath", m_defaultSavePath);
    m_settings->setValue("behavior/autoSave", m_autoSaveResult);
    m_settings->setValue("audio/volume", m_volume);
    m_settings->setValue("reprocess/shaderEnabled", m_autoReprocessShaderEnabled);
    m_settings->setValue("reprocess/aiEnabled", m_autoReprocessAIEnabled);
    m_settings->setValue("video/autoPlay", m_videoAutoPlay);
    m_settings->sync();
}

void SettingsController::loadSettings()
{
    if (!m_settings) return;

    m_theme = m_settings->value("appearance/theme", "dark").toString();
    m_language = m_settings->value("appearance/language", "zh_CN").toString();
    m_sidebarExpanded = m_settings->value("sidebar/expanded", true).toBool();
    m_defaultSavePath = m_settings->value("behavior/defaultSavePath", m_defaultSavePath).toString();
    m_autoSaveResult = m_settings->value("behavior/autoSave", false).toBool();
    m_volume = m_settings->value("audio/volume", 80).toInt();
    m_autoReprocessShaderEnabled = m_settings->value("reprocess/shaderEnabled", true).toBool();
    m_autoReprocessAIEnabled = m_settings->value("reprocess/aiEnabled", true).toBool();
    m_lastExitClean = m_settings->value("system/lastExitClean", true).toBool();
    m_lastExitReason = m_settings->value("system/lastExitReason", QString()).toString();
    m_videoAutoPlay = m_settings->value("video/autoPlay", true).toBool();
}

void SettingsController::resetToDefaults()
{
    m_theme = "dark";
    m_language = "zh_CN";
    m_sidebarExpanded = true;
    QString picturesPath = QStandardPaths::writableLocation(QStandardPaths::PicturesLocation);
    m_defaultSavePath = QDir(picturesPath).filePath("EnhanceVision");

    m_autoSaveResult = false;
    m_volume = 80;
    m_autoReprocessShaderEnabled = true;
    m_autoReprocessAIEnabled = true;
    m_lastExitClean = true;
    m_lastExitReason.clear();
    m_videoAutoPlay = true;

    emit themeChanged();
    emit languageChanged();
    emit sidebarExpandedChanged();
    emit defaultSavePathChanged();
    emit autoSaveResultChanged();
    emit volumeChanged();
    emit autoReprocessShaderEnabledChanged();
    emit autoReprocessAIEnabledChanged();
    emit autoReprocessAllEnabledChanged();
    emit lastExitCleanChanged();
    emit videoAutoPlayChanged();
    emit settingsChanged();
}

QString SettingsController::getSetting(const QString& key, const QString& defaultValue)
{
    if (!m_settings) return defaultValue;
    return m_settings->value(key, defaultValue).toString();
}

void SettingsController::setSetting(const QString& key, const QString& value)
{
    if (!m_settings) return;
    m_settings->setValue(key, value);
    emit settingsChanged();
}

} // namespace EnhanceVision
