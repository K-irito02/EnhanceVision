/**
 * @file SettingsController.cpp
 * @brief 设置控制器实现
 * @author Qt客户端开发工程师
 */

#include "EnhanceVision/controllers/SettingsController.h"
#include "EnhanceVision/controllers/SessionController.h"
#include "EnhanceVision/providers/ThumbnailProvider.h"
#include "EnhanceVision/core/ThumbnailDatabase.h"
#include <QStandardPaths>
#include <QDir>
#include <QDirIterator>
#include <QFileInfo>
#include <QCoreApplication>
#include <QThread>

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
    , m_videoAutoPlayOnSwitch(true)
    , m_videoRestorePosition(true)
    , m_customDataPath()
    , m_aiImageSize(0)
    , m_aiVideoSize(0)
    , m_shaderImageSize(0)
    , m_shaderVideoSize(0)
    , m_logSize(0)
    , m_aiImageFileCount(0)
    , m_aiVideoFileCount(0)
    , m_shaderImageFileCount(0)
    , m_shaderVideoFileCount(0)
{
    QString configPath = QStandardPaths::writableLocation(QStandardPaths::ConfigLocation);
    QString settingsFile = QDir(configPath).filePath("EnhanceVision/settings.ini");
    m_settings = new QSettings(settingsFile, QSettings::IniFormat, this);

    QString picturesPath = QStandardPaths::writableLocation(QStandardPaths::PicturesLocation);
    m_defaultSavePath = QDir(picturesPath).filePath("EnhanceVision");

    loadSettings();
    refreshDataSize();
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
        m_settings->setValue("system/prevAutoReprocessShader", enabled);
        m_settings->sync();
        qInfo() << "[SettingsController] setAutoReprocessShaderEnabled:" << enabled 
                << ", saved to prevAutoReprocessShader";
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
        m_settings->setValue("system/prevAutoReprocessAI", enabled);
        m_settings->sync();
        qInfo() << "[SettingsController] setAutoReprocessAIEnabled:" << enabled 
                << ", saved to prevAutoReprocessAI";
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
        m_settings->setValue("system/prevAutoReprocessShader", enabled);
        m_settings->setValue("system/prevAutoReprocessAI", enabled);
        m_settings->sync();
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

bool SettingsController::videoAutoPlayOnSwitch() const
{
    return m_videoAutoPlayOnSwitch;
}

void SettingsController::setVideoAutoPlayOnSwitch(bool autoPlay)
{
    if (m_videoAutoPlayOnSwitch != autoPlay) {
        m_videoAutoPlayOnSwitch = autoPlay;
        emit videoAutoPlayOnSwitchChanged();
        emit settingsChanged();
        saveSettings();
    }
}

bool SettingsController::videoRestorePosition() const
{
    return m_videoRestorePosition;
}

void SettingsController::setVideoRestorePosition(bool restore)
{
    if (m_videoRestorePosition != restore) {
        m_videoRestorePosition = restore;
        emit videoRestorePositionChanged();
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
    
    bool prevShaderEnabled = m_settings->value("system/prevAutoReprocessShader", true).toBool();
    bool prevAIEnabled = m_settings->value("system/prevAutoReprocessAI", true).toBool();
    
    qInfo() << "[SettingsController] Previous auto-reprocess settings: shader=" << prevShaderEnabled 
            << ", ai=" << prevAIEnabled;
    
    if (!prevShaderEnabled && !prevAIEnabled) {
        qInfo() << "[SettingsController] Auto-reprocess was already disabled before crash, no warning needed";
        return false;
    }
    
    qWarning() << "[SettingsController] Abnormal exit detected! lastExitClean=" << wasClean
               << ", lastExitReason=" << lastReason;
    
    m_crashDetectedOnStartup = true;
    m_autoReprocessShaderEnabled = false;
    m_autoReprocessAIEnabled = false;
    
    m_settings->setValue("reprocess/shaderEnabled", false);
    m_settings->setValue("reprocess/aiEnabled", false);
    m_settings->setValue("system/prevAutoReprocessShader", false);
    m_settings->setValue("system/prevAutoReprocessAI", false);
    m_settings->sync();
    
    emit autoReprocessShaderEnabledChanged();
    emit autoReprocessAIEnabledChanged();
    emit autoReprocessAllEnabledChanged();
    emit crashDetected();
    emit crashDetectedOnStartupChanged();
    
    return true;
}

QString SettingsController::customDataPath() const
{
    return m_customDataPath;
}

void SettingsController::setCustomDataPath(const QString& path)
{
    QString normalizedPath = path.trimmed();
    if (m_customDataPath != normalizedPath) {
        m_customDataPath = normalizedPath;
        emit customDataPathChanged();
        emit settingsChanged();
        saveSettings();
        refreshDataSize();
    }
}

qint64 SettingsController::aiImageSize() const
{
    return m_aiImageSize;
}

qint64 SettingsController::aiVideoSize() const
{
    return m_aiVideoSize;
}

qint64 SettingsController::shaderImageSize() const
{
    return m_shaderImageSize;
}

qint64 SettingsController::shaderVideoSize() const
{
    return m_shaderVideoSize;
}

qint64 SettingsController::logSize() const
{
    return m_logSize;
}

qint64 SettingsController::totalCacheSize() const
{
    return m_aiImageSize + m_aiVideoSize + m_shaderImageSize + m_shaderVideoSize + m_logSize;
}

int SettingsController::thumbnailCacheCount() const
{
    auto* db = ThumbnailDatabase::instance();
    if (db && db->isInitialized()) {
        return db->validCount();
    }
    return 0;
}

qint64 SettingsController::thumbnailCacheSize() const
{
    return m_thumbnailCacheSize;
}

qint64 SettingsController::thumbnailDiskSize() const
{
    return m_thumbnailDiskSize;
}

int SettingsController::aiImageFileCount() const
{
    return m_aiImageFileCount;
}

int SettingsController::aiVideoFileCount() const
{
    return m_aiVideoFileCount;
}

int SettingsController::shaderImageFileCount() const
{
    return m_shaderImageFileCount;
}

int SettingsController::shaderVideoFileCount() const
{
    return m_shaderVideoFileCount;
}

QString SettingsController::effectiveDataPath() const
{
    if (!m_customDataPath.isEmpty()) {
        return m_customDataPath;
    }
    return QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation);
}

qint64 SettingsController::calculateDirectorySize(const QString& path) const
{
    qint64 size = 0;
    QDir dir(path);
    if (!dir.exists()) {
        return 0;
    }
    
    QDirIterator it(path, QDir::Files | QDir::NoSymLinks, QDirIterator::Subdirectories);
    while (it.hasNext()) {
        it.next();
        size += it.fileInfo().size();
    }
    return size;
}

int SettingsController::countFilesInDirectory(const QString& path) const
{
    int count = 0;
    QDir dir(path);
    if (!dir.exists()) {
        return 0;
    }
    
    QDirIterator it(path, QDir::Files | QDir::NoSymLinks, QDirIterator::Subdirectories);
    while (it.hasNext()) {
        it.next();
        ++count;
    }
    return count;
}

bool SettingsController::clearDirectory(const QString& path)
{
    QDir dir(path);
    if (!dir.exists()) {
        return true;
    }
    
    bool success = true;
    QDirIterator it(path, QDir::Files | QDir::Dirs | QDir::NoDotAndDotDot, QDirIterator::Subdirectories);
    QStringList itemsToDelete;
    
    while (it.hasNext()) {
        it.next();
        itemsToDelete.prepend(it.filePath());
    }
    
    for (const QString& item : itemsToDelete) {
        QFileInfo fi(item);
        if (fi.isDir()) {
            if (!QDir(item).removeRecursively()) {
                qWarning() << "[SettingsController] Failed to remove directory:" << item;
                success = false;
            }
        } else {
            bool deleted = false;
            for (int retry = 0; retry < 3 && !deleted; ++retry) {
                if (QFile::remove(item)) {
                    deleted = true;
                } else {
                    if (retry < 2) {
                        QThread::msleep(100);
                    }
                }
            }
            if (!deleted) {
                qWarning() << "[SettingsController] Failed to remove file after 3 retries:" << item;
                success = false;
            }
        }
    }
    
    return success;
}

QString SettingsController::getAIImagePath() const
{
    return effectiveDataPath() + "/ai/images";
}

QString SettingsController::getAIVideoPath() const
{
    return effectiveDataPath() + "/ai/videos";
}

QString SettingsController::getShaderImagePath() const
{
    return effectiveDataPath() + "/shader/images";
}

QString SettingsController::getShaderVideoPath() const
{
    return effectiveDataPath() + "/shader/videos";
}

QString SettingsController::getLogPath() const
{
    return QCoreApplication::applicationDirPath() + "/logs";
}

void SettingsController::refreshDataSize()
{
    m_aiImageSize = calculateDirectorySize(getAIImagePath());
    m_aiVideoSize = calculateDirectorySize(getAIVideoPath());
    m_shaderImageSize = calculateDirectorySize(getShaderImagePath());
    m_shaderVideoSize = calculateDirectorySize(getShaderVideoPath());
    m_logSize = calculateDirectorySize(getLogPath());
    
    m_aiImageFileCount = countFilesInDirectory(getAIImagePath());
    m_aiVideoFileCount = countFilesInDirectory(getAIVideoPath());
    m_shaderImageFileCount = countFilesInDirectory(getShaderImagePath());
    m_shaderVideoFileCount = countFilesInDirectory(getShaderVideoPath());

    auto* provider = ThumbnailProvider::instance();
    if (provider) {
        m_thumbnailCacheSize = provider->memoryCacheSize();
    }

    auto* db = ThumbnailDatabase::instance();
    if (db && db->isInitialized()) {
        m_thumbnailDiskSize = db->validDiskSize();
    }
    
    emit dataSizeChanged();
}

bool SettingsController::clearAIImageData()
{
    QString path = getAIImagePath();
    
    ThumbnailProvider* thumbnailProvider = ThumbnailProvider::instance();
    if (thumbnailProvider) {
        thumbnailProvider->clearThumbnailsByPathPrefix(path);
    }
    
    bool success = clearDirectory(path);
    
    if (success && m_sessionController) {
        m_sessionController->clearMediaFilesByModeAndType(1, 0);  // mode=AI, type=Image
    }
    
    refreshDataSize();
    qInfo() << "[SettingsController] Cleared AI image data:" << path << "success:" << success;
    return success;
}

bool SettingsController::clearAIVideoData()
{
    QString path = getAIVideoPath();
    
    ThumbnailProvider* thumbnailProvider = ThumbnailProvider::instance();
    if (thumbnailProvider) {
        thumbnailProvider->clearThumbnailsByPathPrefix(path);
    }
    
    bool success = clearDirectory(path);
    
    if (success && m_sessionController) {
        m_sessionController->clearMediaFilesByModeAndType(1, 1);  // mode=AI, type=Video
    }
    
    refreshDataSize();
    qInfo() << "[SettingsController] Cleared AI video data:" << path << "success:" << success;
    return success;
}

bool SettingsController::clearShaderImageData()
{
    QString path = getShaderImagePath();
    
    ThumbnailProvider* thumbnailProvider = ThumbnailProvider::instance();
    if (thumbnailProvider) {
        thumbnailProvider->clearThumbnailsByPathPrefix(path);
    }
    
    bool success = clearDirectory(path);
    
    if (success && m_sessionController) {
        m_sessionController->clearMediaFilesByModeAndType(0, 0);  // mode=Shader, type=Image
    }
    
    refreshDataSize();
    qInfo() << "[SettingsController] Cleared Shader image data:" << path << "success:" << success;
    return success;
}

bool SettingsController::clearShaderVideoData()
{
    QString path = getShaderVideoPath();
    
    ThumbnailProvider* thumbnailProvider = ThumbnailProvider::instance();
    if (thumbnailProvider) {
        thumbnailProvider->clearThumbnailsByPathPrefix(path);
    }
    
    bool success = clearDirectory(path);
    
    if (success && m_sessionController) {
        m_sessionController->clearMediaFilesByModeAndType(0, 1);  // mode=Shader, type=Video
    }
    
    refreshDataSize();
    qInfo() << "[SettingsController] Cleared Shader video data:" << path << "success:" << success;
    return success;
}

bool SettingsController::clearLogs()
{
    QString path = getLogPath();
    bool success = clearDirectory(path);
    refreshDataSize();
    qInfo() << "[SettingsController] Cleared logs:" << path << "success:" << success;
    return success;
}

bool SettingsController::clearAllCache()
{
    bool success = true;
    
    ThumbnailProvider* thumbnailProvider = ThumbnailProvider::instance();
    if (thumbnailProvider) {
        thumbnailProvider->clearAll();
    }

    ThumbnailDatabase* db = ThumbnailDatabase::instance();
    if (db && db->isInitialized()) {
        db->clearAll();
    }
    
    success &= clearDirectory(getAIImagePath());
    success &= clearDirectory(getAIVideoPath());
    success &= clearDirectory(getShaderImagePath());
    success &= clearDirectory(getShaderVideoPath());
    success &= clearDirectory(getLogPath());
    
    if (success && m_sessionController) {
        m_sessionController->clearAllSessionMessages();
    }
    
    refreshDataSize();
    qInfo() << "[SettingsController] Cleared all cache, success:" << success;
    return success;
}

bool SettingsController::clearThumbnailCache()
{
    ThumbnailProvider* provider = ThumbnailProvider::instance();
    if (provider) {
        provider->clearAll();
    }
    
    ThumbnailDatabase* db = ThumbnailDatabase::instance();
    int removed = 0;
    if (db && db->isInitialized()) {
        removed = db->clearAll();
        qInfo() << "[SettingsController] Cleared" << removed << "thumbnail metadata entries";
    }
    
    refreshDataSize();
    qInfo() << "[SettingsController] Cleared thumbnail cache successfully";
    return true;
}

QString SettingsController::getThumbnailCachePath() const
{
    auto* db = ThumbnailDatabase::instance();
    if (db && db->isInitialized()) {
        return db->thumbnailDirPath();
    }
    return effectiveDataPath() + "/thumbnails";
}

bool SettingsController::checkThumbnailStorageThreshold(double thresholdGB)
{
    qint64 currentBytes = thumbnailDiskSize();
    qint64 thresholdBytes = static_cast<qint64>(thresholdGB * 1024.0 * 1024.0 * 1024.0);
    return currentBytes > thresholdBytes;
}

QString SettingsController::formatSize(qint64 bytes) const
{
    if (bytes < 0) {
        return QStringLiteral("0 B");
    }
    
    const qint64 KB = 1024;
    const qint64 MB = 1024 * KB;
    const qint64 GB = 1024 * MB;
    
    if (bytes >= GB) {
        return QStringLiteral("%1 GB").arg(bytes / (double)GB, 0, 'f', 2);
    } else if (bytes >= MB) {
        return QStringLiteral("%1 MB").arg(bytes / (double)MB, 0, 'f', 1);
    } else if (bytes >= KB) {
        return QStringLiteral("%1 KB").arg(bytes / (double)KB, 0, 'f', 1);
    } else {
        return QStringLiteral("%1 B").arg(bytes);
    }
}

void SettingsController::saveSettings()
{
    if (!m_settings) return;

    m_settings->setValue("appearance/theme", m_theme);
    m_settings->setValue("appearance/language", m_language);
    m_settings->setValue("sidebar/expanded", m_sidebarExpanded);
    m_settings->setValue("behavior/defaultSavePath", m_defaultSavePath);
    m_settings->setValue("behavior/autoSave", m_autoSaveResult);
    m_settings->setValue("behavior/customDataPath", m_customDataPath);
    m_settings->setValue("audio/volume", m_volume);
    m_settings->setValue("reprocess/shaderEnabled", m_autoReprocessShaderEnabled);
    m_settings->setValue("reprocess/aiEnabled", m_autoReprocessAIEnabled);
    m_settings->setValue("video/autoPlay", m_videoAutoPlay);
    m_settings->setValue("video/autoPlayOnSwitch", m_videoAutoPlayOnSwitch);
    m_settings->setValue("video/restorePosition", m_videoRestorePosition);
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
    m_customDataPath = m_settings->value("behavior/customDataPath", QString()).toString();
    m_volume = m_settings->value("audio/volume", 80).toInt();
    m_autoReprocessShaderEnabled = m_settings->value("reprocess/shaderEnabled", true).toBool();
    m_autoReprocessAIEnabled = m_settings->value("reprocess/aiEnabled", true).toBool();
    m_lastExitClean = m_settings->value("system/lastExitClean", true).toBool();
    m_lastExitReason = m_settings->value("system/lastExitReason", QString()).toString();
    m_videoAutoPlay = m_settings->value("video/autoPlay", true).toBool();
    m_videoAutoPlayOnSwitch = m_settings->value("video/autoPlayOnSwitch", true).toBool();
    m_videoRestorePosition = m_settings->value("video/restorePosition", true).toBool();
    
    if (!m_settings->contains("system/prevAutoReprocessShader")) {
        m_settings->setValue("system/prevAutoReprocessShader", m_autoReprocessShaderEnabled);
    }
    if (!m_settings->contains("system/prevAutoReprocessAI")) {
        m_settings->setValue("system/prevAutoReprocessAI", m_autoReprocessAIEnabled);
    }
    m_settings->sync();
}

void SettingsController::resetToDefaults()
{
    m_theme = "dark";
    m_language = "zh_CN";
    m_sidebarExpanded = true;
    QString picturesPath = QStandardPaths::writableLocation(QStandardPaths::PicturesLocation);
    m_defaultSavePath = QDir(picturesPath).filePath("EnhanceVision");
    m_customDataPath.clear();

    m_autoSaveResult = false;
    m_volume = 80;
    m_autoReprocessShaderEnabled = true;
    m_autoReprocessAIEnabled = true;
    m_lastExitClean = true;
    m_lastExitReason.clear();
    m_videoAutoPlay = true;
    m_videoAutoPlayOnSwitch = true;
    m_videoRestorePosition = true;

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
    emit videoAutoPlayOnSwitchChanged();
    emit videoRestorePositionChanged();
    emit customDataPathChanged();
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

void SettingsController::setSessionController(SessionController* controller)
{
    m_sessionController = controller;
}

} // namespace EnhanceVision
