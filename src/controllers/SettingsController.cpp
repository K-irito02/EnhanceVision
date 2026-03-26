/**
 * @file SettingsController.cpp
 * @brief 设置控制器实现
 * @author Qt客户端开发工程师
 */

#include "EnhanceVision/controllers/SettingsController.h"
#include "EnhanceVision/core/ResourceManager.h"
#include <QStandardPaths>
#include <QDir>

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
    , m_maxConcurrentTasks(2)
    , m_maxConcurrentSessions(1)
    , m_maxConcurrentFilesPerMessage(2)
    , m_autoSaveResult(false)
    , m_volume(80)
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

int SettingsController::maxConcurrentTasks() const
{
    return m_maxConcurrentTasks;
}

void SettingsController::setMaxConcurrentTasks(int count)
{
    if (m_maxConcurrentTasks != count && count > 0) {
        m_maxConcurrentTasks = count;
        emit maxConcurrentTasksChanged();
        emit settingsChanged();
    }
}

int SettingsController::maxConcurrentSessions() const
{
    return m_maxConcurrentSessions;
}

void SettingsController::setMaxConcurrentSessions(int count)
{
    count = qBound(1, count, 4);
    if (m_maxConcurrentSessions != count) {
        m_maxConcurrentSessions = count;
        // 自动同步综合并发数
        int effective = m_maxConcurrentSessions * m_maxConcurrentFilesPerMessage;
        setMaxConcurrentTasks(effective);
        emit maxConcurrentSessionsChanged();
        emit settingsChanged();
        saveSettings();
    }
}

int SettingsController::maxConcurrentFilesPerMessage() const
{
    return m_maxConcurrentFilesPerMessage;
}

void SettingsController::setMaxConcurrentFilesPerMessage(int count)
{
    count = qBound(1, count, 4);
    if (m_maxConcurrentFilesPerMessage != count) {
        m_maxConcurrentFilesPerMessage = count;
        int effective = m_maxConcurrentSessions * m_maxConcurrentFilesPerMessage;
        setMaxConcurrentTasks(effective);
        emit maxConcurrentFilesPerMessageChanged();
        emit settingsChanged();
        saveSettings();
    }
}

QString SettingsController::devicePerformanceHint(int sessions, int filesPerMsg) const
{
    int total = qMax(1, sessions) * qMax(1, filesPerMsg);
    
    // 获取真实系统资源数据
    auto* resourceMgr = ResourceManager::instance();
    const qint64 totalMemoryMB = resourceMgr->totalSystemMemoryMB();
    const qint64 availableMemoryMB = resourceMgr->availableSystemMemoryMB();
    const qreal memoryUsagePercent = totalMemoryMB > 0 ? 
        static_cast<qreal>(totalMemoryMB - availableMemoryMB) / totalMemoryMB * 100.0 : 0;
    
    // 获取 GPU 显存信息
    qint64 gpuTotalMB = 0;
    qint64 gpuAvailableMB = 0;
    
#ifdef Q_OS_WIN
    IDXGIFactory* factory = nullptr;
    if (SUCCEEDED(CreateDXGIFactory(__uuidof(IDXGIFactory), reinterpret_cast<void**>(&factory)))) {
        IDXGIAdapter* adapter = nullptr;
        if (SUCCEEDED(factory->EnumAdapters(0, &adapter))) {
            DXGI_ADAPTER_DESC desc;
            if (SUCCEEDED(adapter->GetDesc(&desc))) {
                gpuTotalMB = static_cast<qint64>(desc.DedicatedVideoMemory / (1024 * 1024));
            }
            adapter->Release();
        }
        factory->Release();
    }
    
    // 估算可用显存（基于已用内存和并发任务数）
    const int activeTasks = resourceMgr->activeTaskCount();
    const qint64 estimatedUsedGpuMB = activeTasks * 512;  // 估算每任务约 512MB
    gpuAvailableMB = qMax(0LL, gpuTotalMB - estimatedUsedGpuMB);
#endif

    // 根据实际设备能力评估并发设置
    QString hint;
    const qint64 estimatedMemoryPerTask = 512;  // MB
    const qint64 requiredMemory = total * estimatedMemoryPerTask;
    const qint64 requiredGpuMemory = total * 512;  // MB
    
    // 构建状态信息
    QString memoryStatus = tr("内存: %1/%2 GB (已用 %3%)")
        .arg((totalMemoryMB - availableMemoryMB) / 1024.0, 0, 'f', 1)
        .arg(totalMemoryMB / 1024.0, 0, 'f', 1)
        .arg(memoryUsagePercent, 0, 'f', 0);
    
    QString gpuStatus;
    if (gpuTotalMB > 0) {
        gpuStatus = tr(" | 显存: %1 GB").arg(gpuTotalMB / 1024.0, 0, 'f', 1);
    }
    
    // 综合评估
    bool memoryOk = availableMemoryMB >= requiredMemory;
    bool gpuOk = gpuTotalMB == 0 || gpuTotalMB >= requiredGpuMemory;
    
    if (total <= 2 && memoryOk && gpuOk) {
        hint = tr("[OK] 流畅 - 资源充足");
    } else if (total <= 4 && memoryOk) {
        hint = tr("[!] 轻微负载 - 可正常运行");
    } else if (!memoryOk || (gpuTotalMB > 0 && gpuTotalMB < 4096 && total > 4)) {
        hint = tr("[!!] 高负载 - 可能出现卡顿");
    } else if (total > 8 || memoryUsagePercent > 85) {
        hint = tr("[!!!] 资源紧张 - 建议降低并发数");
    } else {
        hint = tr("[!] 中等负载 - 高端设备可正常运行");
    }
    
    return hint + "\n" + memoryStatus + gpuStatus;
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

void SettingsController::saveSettings()
{
    if (!m_settings) return;

    m_settings->setValue("appearance/theme", m_theme);
    m_settings->setValue("appearance/language", m_language);
    m_settings->setValue("sidebar/expanded", m_sidebarExpanded);
    m_settings->setValue("performance/maxConcurrent", m_maxConcurrentTasks);
    m_settings->setValue("performance/maxConcurrentSessions", m_maxConcurrentSessions);
    m_settings->setValue("performance/maxConcurrentFilesPerMessage", m_maxConcurrentFilesPerMessage);
    m_settings->setValue("behavior/defaultSavePath", m_defaultSavePath);
    m_settings->setValue("behavior/autoSave", m_autoSaveResult);
    m_settings->setValue("audio/volume", m_volume);
    m_settings->sync();
}

void SettingsController::loadSettings()
{
    if (!m_settings) return;

    m_theme = m_settings->value("appearance/theme", "dark").toString();
    m_language = m_settings->value("appearance/language", "zh_CN").toString();
    m_sidebarExpanded = m_settings->value("sidebar/expanded", true).toBool();
    m_maxConcurrentTasks = m_settings->value("performance/maxConcurrent", 2).toInt();
    m_maxConcurrentSessions = qBound(1, m_settings->value("performance/maxConcurrentSessions", 1).toInt(), 4);
    m_maxConcurrentFilesPerMessage = qBound(1, m_settings->value("performance/maxConcurrentFilesPerMessage", 2).toInt(), 4);
    m_defaultSavePath = m_settings->value("behavior/defaultSavePath", m_defaultSavePath).toString();
    m_autoSaveResult = m_settings->value("behavior/autoSave", false).toBool();
    m_volume = m_settings->value("audio/volume", 80).toInt();
}

void SettingsController::resetToDefaults()
{
    m_theme = "dark";
    m_language = "zh_CN";
    m_sidebarExpanded = true;
    m_maxConcurrentTasks = 2;
    m_maxConcurrentSessions = 1;
    m_maxConcurrentFilesPerMessage = 2;

    QString picturesPath = QStandardPaths::writableLocation(QStandardPaths::PicturesLocation);
    m_defaultSavePath = QDir(picturesPath).filePath("EnhanceVision");

    m_autoSaveResult = false;
    m_volume = 80;

    emit themeChanged();
    emit languageChanged();
    emit sidebarExpandedChanged();
    emit maxConcurrentTasksChanged();
    emit maxConcurrentSessionsChanged();
    emit maxConcurrentFilesPerMessageChanged();
    emit defaultSavePathChanged();
    emit autoSaveResultChanged();
    emit volumeChanged();
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
