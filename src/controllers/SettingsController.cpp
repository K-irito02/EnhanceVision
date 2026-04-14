/**
 * @file SettingsController.cpp
 * @brief 设置控制器实现
 * @author Qt客户端开发工程师
 */

#include "EnhanceVision/controllers/SettingsController.h"
#include "EnhanceVision/controllers/SessionController.h"
#include "EnhanceVision/services/InstallMaintenanceService.h"
#include "EnhanceVision/utils/StoragePaths.h"
#include "EnhanceVision/providers/ThumbnailProvider.h"
#include "EnhanceVision/core/ThumbnailDatabase.h"
#include <QStandardPaths>
#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QCoreApplication>
#include <QThread>
#include <QDateTime>
#include <QRegularExpression>

namespace EnhanceVision {

namespace {

QVariantMap buildCacheClearSummary(const QString& reason,
                                   bool success,
                                   int removedFiles = 0,
                                   int removedMessages = 0,
                                   int cancelledTasks = 0,
                                   int sessionsAffected = 0,
                                   const QString& viewerImpact = QStringLiteral("unchanged"))
{
    QVariantMap summary;
    summary[QStringLiteral("reason")] = reason;
    summary[QStringLiteral("removedFiles")] = removedFiles;
    summary[QStringLiteral("removedMessages")] = removedMessages;
    summary[QStringLiteral("cancelledTasks")] = cancelledTasks;
    summary[QStringLiteral("sessionsAffected")] = sessionsAffected;
    summary[QStringLiteral("viewerImpact")] = viewerImpact;
    summary[QStringLiteral("success")] = success;
    return summary;
}

int countFilesRecursively(const QString& path)
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

qint64 calculateDirectorySizeRecursively(const QString& path)
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

void enrichDiskCleanupSummary(QVariantMap& summary, const QStringList& paths, bool diskCleanupOk)
{
    QVariantList residualEntries;
    int residualFiles = 0;
    qint64 residualBytes = 0;

    for (const QString& path : paths) {
        const int files = countFilesRecursively(path);
        const qint64 bytes = calculateDirectorySizeRecursively(path);
        if (files <= 0 && bytes <= 0) {
            continue;
        }

        QVariantMap entry;
        entry[QStringLiteral("path")] = path;
        entry[QStringLiteral("files")] = files;
        entry[QStringLiteral("bytes")] = bytes;
        residualEntries.append(entry);

        residualFiles += files;
        residualBytes += bytes;
    }

    const bool hasResidualData = residualFiles > 0 || residualBytes > 0;
    const bool baseSuccess = summary.value(QStringLiteral("success"), true).toBool();

    summary[QStringLiteral("diskCleanupOk")] = diskCleanupOk;
    summary[QStringLiteral("hasResidualData")] = hasResidualData;
    summary[QStringLiteral("residualFiles")] = residualFiles;
    summary[QStringLiteral("residualBytes")] = residualBytes;
    summary[QStringLiteral("residualEntries")] = residualEntries;
    summary[QStringLiteral("success")] = baseSuccess && !hasResidualData;
}

QString translatedStorageMessage(const char* text)
{
    return QCoreApplication::translate("SettingsController", text);
}

QString applicationConfigRootPath()
{
    return StoragePaths::configRootPath();
}

QString normalizeDirectoryPathImpl(const QString& path)
{
    return StoragePaths::normalizeDirectoryPath(path);
}

bool isStrictAbsolutePath(const QString& normalizedPath)
{
    if (normalizedPath.isEmpty()) {
        return false;
    }

#ifdef Q_OS_WIN
    // Accept drive absolute (e.g. E:/foo) and UNC (e.g. //server/share).
    static const QRegularExpression driveAbsolutePattern(
        QStringLiteral("^[A-Za-z]:/.*$"),
        QRegularExpression::CaseInsensitiveOption);
    static const QRegularExpression uncPattern(QStringLiteral("^//[^/]+/[^/]+.*$"));
    return driveAbsolutePattern.match(normalizedPath).hasMatch()
        || uncPattern.match(normalizedPath).hasMatch();
#else
    return normalizedPath.startsWith(QLatin1Char('/'));
#endif
}

QString normalizedAppDir(const QString& applicationDirPath)
{
    return normalizeDirectoryPathImpl(applicationDirPath);
}

bool pathEqualsOrIsChildOf(const QString& candidate, const QString& base)
{
    if (candidate.isEmpty() || base.isEmpty()) {
        return false;
    }

#ifdef Q_OS_WIN
    const Qt::CaseSensitivity sensitivity = Qt::CaseInsensitive;
#else
    const Qt::CaseSensitivity sensitivity = Qt::CaseSensitive;
#endif

    if (QString::compare(candidate, base, sensitivity) == 0) {
        return true;
    }

    const QString normalizedBase = base.endsWith(QLatin1Char('/'))
        ? base
        : base + QLatin1Char('/');
    return candidate.startsWith(normalizedBase, sensitivity);
}

QString nearestExistingAncestorPath(const QString& path)
{
    QString current = normalizeDirectoryPathImpl(path);
    while (!current.isEmpty()) {
        QFileInfo info(current);
        if (info.exists()) {
            return current;
        }

        const QString parent = QFileInfo(current).dir().absolutePath();
        const QString normalizedParent = normalizeDirectoryPathImpl(parent);
        if (normalizedParent == current) {
            break;
        }
        current = normalizedParent;
    }

    return {};
}

bool isProtectedDirectoryPath(const QString& path, const QString& applicationDirPath, QString* reason)
{
    const QString normalizedPath = normalizeDirectoryPathImpl(path);
    Q_UNUSED(applicationDirPath)

#ifdef Q_OS_WIN
    // Block Windows protected roots both by environment variables and by generic
    // drive-root names (e.g. E:/Program Files), so non-system drives are covered.
    const QStringList blockedRoots = {
        normalizeDirectoryPathImpl(qEnvironmentVariable("ProgramFiles")),
        normalizeDirectoryPathImpl(qEnvironmentVariable("ProgramFiles(x86)")),
        normalizeDirectoryPathImpl(qEnvironmentVariable("ProgramW6432")),
        normalizeDirectoryPathImpl(qEnvironmentVariable("SystemRoot")),
        normalizeDirectoryPathImpl(qEnvironmentVariable("windir"))
    };

    for (const QString& blockedRoot : blockedRoots) {
        if (!blockedRoot.isEmpty() && pathEqualsOrIsChildOf(normalizedPath, blockedRoot)) {
            if (reason) {
                *reason = translatedStorageMessage("该目录属于 Windows 受保护目录，请改为普通可写目录");
            }
            return true;
        }
    }

    const QRegularExpression driveProtectedPattern(
        QStringLiteral("^[A-Za-z]:/(Program Files( \\(x86\\))?|Windows)(/|$)"),
        QRegularExpression::CaseInsensitiveOption);
    if (driveProtectedPattern.match(normalizedPath).hasMatch()) {
        if (reason) {
            *reason = translatedStorageMessage("该目录属于 Windows 受保护目录，请改为普通可写目录");
        }
        return true;
    }
#endif

    return false;
}

bool isDirectoryPathUsable(const QString& path, const QString& applicationDirPath, QString* reason)
{
    const QString normalizedPath = normalizeDirectoryPathImpl(path);
    if (normalizedPath.isEmpty()) {
        if (reason) {
            *reason = translatedStorageMessage("目录为空");
        }
        return false;
    }

    if (!QDir::isAbsolutePath(normalizedPath) || !isStrictAbsolutePath(normalizedPath)) {
        if (reason) {
            *reason = translatedStorageMessage("目录必须是绝对路径");
        }
        return false;
    }

    if (isProtectedDirectoryPath(normalizedPath, applicationDirPath, reason)) {
        return false;
    }

    QFileInfo info(normalizedPath);
    if (info.exists()) {
        if (!info.isDir()) {
            if (reason) {
                *reason = translatedStorageMessage("路径已存在但不是目录");
            }
            return false;
        }
        if (!info.isWritable()) {
            if (reason) {
                *reason = translatedStorageMessage("目录不可写");
            }
            return false;
        }
        return true;
    }

    const QString ancestor = nearestExistingAncestorPath(normalizedPath);
    if (ancestor.isEmpty()) {
        if (reason) {
            *reason = translatedStorageMessage("无法找到可访问的父目录");
        }
        return false;
    }

    QFileInfo ancestorInfo(ancestor);
    if (!ancestorInfo.isDir() || !ancestorInfo.isWritable()) {
        if (reason) {
            *reason = translatedStorageMessage("父目录不可写");
        }
        return false;
    }

    return true;
}

bool hasAnyDataRecursively(const QString& path)
{
    QDir dir(path);
    if (!dir.exists()) {
        return false;
    }

    QDirIterator it(path, QDir::Files | QDir::Dirs | QDir::NoDotAndDotDot, QDirIterator::Subdirectories);
    return it.hasNext();
}

QString uniqueMigrationSuffix()
{
    return QDateTime::currentDateTimeUtc().toString(QStringLiteral("yyyyMMddHHmmsszzz"));
}

QString uniqueConflictFilePath(const QString& destinationFilePath)
{
    QFileInfo info(destinationFilePath);
    const QString directoryPath = info.dir().absolutePath();
    const QString baseName = info.completeBaseName();
    const QString suffix = info.suffix();

    QString candidate;
    int counter = 1;
    do {
        const QString serial = QStringLiteral("%1_%2")
            .arg(uniqueMigrationSuffix())
            .arg(counter++);
        const QString fileName = suffix.isEmpty()
            ? QStringLiteral("%1_legacy_%2").arg(baseName, serial)
            : QStringLiteral("%1_legacy_%2.%3").arg(baseName, serial, suffix);
        candidate = QDir(directoryPath).filePath(fileName);
    } while (QFileInfo::exists(candidate));

    return candidate;
}

bool copyFileConflictAware(const QString& sourceFilePath, const QString& destinationFilePath)
{
    QFileInfo sourceInfo(sourceFilePath);
    if (!sourceInfo.exists() || !sourceInfo.isFile()) {
        return false;
    }

    QFileInfo destinationInfo(destinationFilePath);
    QDir destinationDir = destinationInfo.dir();
    if (!destinationDir.exists() && !destinationDir.mkpath(QStringLiteral("."))) {
        return false;
    }

    QString targetPath = destinationFilePath;
    if (QFileInfo::exists(targetPath)) {
        QFileInfo existingInfo(targetPath);
        if (existingInfo.size() == sourceInfo.size()) {
            return true;
        }
        targetPath = uniqueConflictFilePath(destinationFilePath);
    }

    return QFile::copy(sourceFilePath, targetPath);
}

bool copyDirectoryRecursivelyConflictAware(const QString& sourceDirectoryPath, const QString& destinationDirectoryPath)
{
    QDir sourceDir(sourceDirectoryPath);
    if (!sourceDir.exists()) {
        return true;
    }

    QDir destinationDir(destinationDirectoryPath);
    if (!destinationDir.exists() && !destinationDir.mkpath(QStringLiteral("."))) {
        return false;
    }

    const QFileInfoList entries = sourceDir.entryInfoList(
        QDir::Files | QDir::Dirs | QDir::NoDotAndDotDot | QDir::Hidden);
    for (const QFileInfo& entry : entries) {
        const QString sourcePath = entry.absoluteFilePath();
        const QString destinationPath = destinationDir.filePath(entry.fileName());
        if (entry.isDir()) {
            if (!copyDirectoryRecursivelyConflictAware(sourcePath, destinationPath)) {
                return false;
            }
        } else if (!copyFileConflictAware(sourcePath, destinationPath)) {
            return false;
        }
    }

    return true;
}

} // namespace

SettingsController* SettingsController::s_instance = nullptr;

SettingsController::SettingsController(QObject* parent)
    : QObject(parent)
    , m_settings(nullptr)
    , m_theme("dark")
    , m_language("zh_CN")
    , m_sidebarExpanded(true)
    , m_defaultSavePath(defaultConfiguredSavePath())
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
    , m_pauseMode(1)  // 默认模式二：顺序暂停
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
    m_settings = new QSettings(settingsFilePath(), QSettings::IniFormat, this);

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

QString SettingsController::settingsFilePath()
{
    return QDir(applicationConfigRootPath()).filePath(QStringLiteral("settings.ini"));
}

QString SettingsController::configRootPath()
{
    return applicationConfigRootPath();
}

QString SettingsController::defaultConfiguredDataPath()
{
    return QDir(applicationConfigRootPath()).filePath(QStringLiteral("data"));
}

QString SettingsController::defaultConfiguredSavePath()
{
    QString picturesPath = QStandardPaths::writableLocation(QStandardPaths::PicturesLocation);
    if (picturesPath.isEmpty()) {
        picturesPath = QDir::homePath();
    }
    return QDir(picturesPath).filePath(QStringLiteral("EnhanceVision"));
}

QString SettingsController::normalizeDirectoryPath(const QString& path)
{
    return normalizeDirectoryPathImpl(path);
}

QString SettingsController::resolveEffectiveDataPath(const QString& configuredPath,
                                                     bool* fallbackActive,
                                                     QString* fallbackReason,
                                                     const QString& applicationDirPath)
{
    const QString normalizedConfiguredPath = normalizeDirectoryPathImpl(configuredPath);
    if (normalizedConfiguredPath.isEmpty()) {
        if (fallbackActive) {
            *fallbackActive = false;
        }
        if (fallbackReason) {
            fallbackReason->clear();
        }
        return defaultConfiguredDataPath();
    }

    QString validationReason;
    if (isDirectoryPathUsable(normalizedConfiguredPath, applicationDirPath, &validationReason)) {
        if (fallbackActive) {
            *fallbackActive = false;
        }
        if (fallbackReason) {
            fallbackReason->clear();
        }
        return normalizedConfiguredPath;
    }

    if (fallbackActive) {
        *fallbackActive = true;
    }
    if (fallbackReason) {
        *fallbackReason = validationReason;
    }
    return defaultConfiguredDataPath();
}

QString SettingsController::resolveEffectiveSavePath(const QString& configuredPath,
                                                     bool* fallbackActive,
                                                     QString* fallbackReason,
                                                     const QString& applicationDirPath)
{
    const QString normalizedConfiguredPath = normalizeDirectoryPathImpl(configuredPath);
    const QString candidate = normalizedConfiguredPath.isEmpty()
        ? defaultConfiguredSavePath()
        : normalizedConfiguredPath;

    QString validationReason;
    if (isDirectoryPathUsable(candidate, applicationDirPath, &validationReason)) {
        if (fallbackActive) {
            *fallbackActive = false;
        }
        if (fallbackReason) {
            fallbackReason->clear();
        }
        return candidate;
    }

    if (fallbackActive) {
        *fallbackActive = true;
    }
    if (fallbackReason) {
        *fallbackReason = validationReason;
    }
    return defaultConfiguredSavePath();
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
    return m_effectiveDefaultSavePath;
}

void SettingsController::setDefaultSavePath(const QString& path)
{
    const QString normalizedPath = normalizeDirectoryPath(path);
    if (m_defaultSavePath != normalizedPath) {
        m_defaultSavePath = normalizedPath;
        refreshResolvedStoragePaths();
        emit settingsChanged();
        saveSettings();
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

int SettingsController::pauseMode() const
{
    return m_pauseMode;
}

void SettingsController::setPauseMode(int mode)
{
    int clampedMode = qBound(0, mode, 2);
    if (m_pauseMode != clampedMode) {
        m_pauseMode = clampedMode;
        emit pauseModeChanged();
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
}

bool SettingsController::checkAndHandleCrashRecovery()
{
    bool wasClean = m_settings->value("system/lastExitClean", true).toBool();
    QString lastReason = m_settings->value("system/lastExitReason", QString()).toString();
    
    if (wasClean) {
        return false;
    }
    
    QStringList normalReasons = {
        QStringLiteral("normal"),
        QStringLiteral("main_window_closed"),
        QStringLiteral("user_request")
    };
    
    if (!lastReason.isEmpty() && normalReasons.contains(lastReason)) {
        return false;
    }

    qWarning() << "[SettingsController] Abnormal exit detected! lastExitClean=" << wasClean
               << ", lastExitReason=" << lastReason;

    m_crashDetectedOnStartup = true;
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
    changeCustomDataPath(path);
}

bool SettingsController::changeCustomDataPath(const QString& path)
{
    const QString normalizedPath = normalizeDirectoryPath(path);
    if (m_customDataPath == normalizedPath) {
        return true;
    }

    const QString previousConfiguredPath = m_customDataPath;
    const QString previousEffectiveDataPath = m_effectiveDataPath;

    m_customDataPath = normalizedPath;
    refreshResolvedStoragePaths();
    const QString nextEffectiveDataPath = m_effectiveDataPath;

    if (!previousEffectiveDataPath.isEmpty() &&
        !nextEffectiveDataPath.isEmpty() &&
        previousEffectiveDataPath != nextEffectiveDataPath) {
        QString migrationError;
        if (!InstallMaintenanceService::migrateRuntimeData(
                previousEffectiveDataPath,
                nextEffectiveDataPath,
                &migrationError)) {
            qWarning() << "[SettingsController] Failed to migrate runtime data path:"
                       << previousEffectiveDataPath << "->" << nextEffectiveDataPath
                       << migrationError;
            m_customDataPath = previousConfiguredPath;
            refreshResolvedStoragePaths();
            return false;
        }

        if (m_sessionController) {
            m_sessionController->remapStorageRootPaths(previousEffectiveDataPath, nextEffectiveDataPath);
        }

        if (auto* provider = ThumbnailProvider::instance()) {
            provider->clearAll();
        }
        if (auto* db = ThumbnailDatabase::instance(); db && db->isInitialized()) {
            db->close();
            db->initialize(nextEffectiveDataPath);
        }
    }

    emit customDataPathChanged();
    emit storagePathStateChanged();
    emit settingsChanged();
    saveSettings();
    refreshDataSize();
    return true;
}

QString SettingsController::effectiveDefaultSavePath() const
{
    return m_effectiveDefaultSavePath;
}

bool SettingsController::dataPathFallbackActive() const
{
    return m_dataPathFallbackActive;
}

QString SettingsController::dataPathFallbackReason() const
{
    return m_dataPathFallbackReason;
}

bool SettingsController::defaultSavePathFallbackActive() const
{
    return m_defaultSavePathFallbackActive;
}

QString SettingsController::defaultSavePathFallbackReason() const
{
    return m_defaultSavePathFallbackReason;
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
    return m_aiImageSize + m_aiVideoSize + m_shaderImageSize + m_shaderVideoSize +
           m_logSize + m_thumbnailDiskSize;
}

QVariantMap SettingsController::lastCacheClearSummary() const
{
    return m_lastCacheClearSummary;
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
    return m_effectiveDataPath;
}

void SettingsController::refreshResolvedStoragePaths()
{
    const QString applicationDirPath = QCoreApplication::applicationDirPath();

    bool nextDataFallbackActive = false;
    QString nextDataFallbackReason;
    const QString nextEffectiveDataPath = resolveEffectiveDataPath(
        m_customDataPath,
        &nextDataFallbackActive,
        &nextDataFallbackReason,
        applicationDirPath);

    bool nextSaveFallbackActive = false;
    QString nextSaveFallbackReason;
    const QString nextEffectiveSavePath = resolveEffectiveSavePath(
        m_defaultSavePath,
        &nextSaveFallbackActive,
        &nextSaveFallbackReason,
        applicationDirPath);

    const bool storageStateChanged =
        m_effectiveDataPath != nextEffectiveDataPath ||
        m_effectiveDefaultSavePath != nextEffectiveSavePath ||
        m_dataPathFallbackActive != nextDataFallbackActive ||
        m_dataPathFallbackReason != nextDataFallbackReason ||
        m_defaultSavePathFallbackActive != nextSaveFallbackActive ||
        m_defaultSavePathFallbackReason != nextSaveFallbackReason;

    const bool effectiveSavePathChanged = m_effectiveDefaultSavePath != nextEffectiveSavePath;

    m_effectiveDataPath = nextEffectiveDataPath;
    m_effectiveDefaultSavePath = nextEffectiveSavePath;
    m_dataPathFallbackActive = nextDataFallbackActive;
    m_dataPathFallbackReason = nextDataFallbackReason;
    m_defaultSavePathFallbackActive = nextSaveFallbackActive;
    m_defaultSavePathFallbackReason = nextSaveFallbackReason;

    if (effectiveSavePathChanged) {
        emit defaultSavePathChanged();
    }
    if (storageStateChanged) {
        emit storagePathStateChanged();
    }
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
    return QDir(effectiveDataPath()).filePath(QStringLiteral("logs"));
}

void SettingsController::refreshDataSize()
{
    refreshResolvedStoragePaths();
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
    
    const bool diskSuccess = clearDirectory(path);
    
    if (m_sessionController) {
        m_lastCacheClearSummary = m_sessionController->pruneMediaFilesByModeAndType(
            static_cast<int>(ProcessingMode::AIInference),
            static_cast<int>(MediaType::Image),
            QStringLiteral("cache-ai-image"));
        m_lastCacheClearSummary[QStringLiteral("viewerImpact")] = QStringLiteral("sync");
    } else {
        m_lastCacheClearSummary = buildCacheClearSummary(
            QStringLiteral("cache-ai-image"), true, 0, 0, 0, 0, QStringLiteral("sync"));
    }
    enrichDiskCleanupSummary(m_lastCacheClearSummary, {path}, diskSuccess);
    
    refreshDataSize();
    emit lastCacheClearSummaryChanged();
    return m_lastCacheClearSummary.value(QStringLiteral("success")).toBool();
}

bool SettingsController::clearAIVideoData()
{
    QString path = getAIVideoPath();
    
    ThumbnailProvider* thumbnailProvider = ThumbnailProvider::instance();
    if (thumbnailProvider) {
        thumbnailProvider->clearThumbnailsByPathPrefix(path);
    }
    
    const bool diskSuccess = clearDirectory(path);
    
    if (m_sessionController) {
        m_lastCacheClearSummary = m_sessionController->pruneMediaFilesByModeAndType(
            static_cast<int>(ProcessingMode::AIInference),
            static_cast<int>(MediaType::Video),
            QStringLiteral("cache-ai-video"));
        m_lastCacheClearSummary[QStringLiteral("viewerImpact")] = QStringLiteral("sync");
    } else {
        m_lastCacheClearSummary = buildCacheClearSummary(
            QStringLiteral("cache-ai-video"), true, 0, 0, 0, 0, QStringLiteral("sync"));
    }
    enrichDiskCleanupSummary(m_lastCacheClearSummary, {path}, diskSuccess);
    
    refreshDataSize();
    emit lastCacheClearSummaryChanged();
    return m_lastCacheClearSummary.value(QStringLiteral("success")).toBool();
}

bool SettingsController::clearShaderImageData()
{
    QString path = getShaderImagePath();
    
    ThumbnailProvider* thumbnailProvider = ThumbnailProvider::instance();
    if (thumbnailProvider) {
        thumbnailProvider->clearThumbnailsByPathPrefix(path);
    }
    
    const bool diskSuccess = clearDirectory(path);
    
    if (m_sessionController) {
        m_lastCacheClearSummary = m_sessionController->pruneMediaFilesByModeAndType(
            static_cast<int>(ProcessingMode::Shader),
            static_cast<int>(MediaType::Image),
            QStringLiteral("cache-shader-image"));
        m_lastCacheClearSummary[QStringLiteral("viewerImpact")] = QStringLiteral("sync");
    } else {
        m_lastCacheClearSummary = buildCacheClearSummary(
            QStringLiteral("cache-shader-image"), true, 0, 0, 0, 0, QStringLiteral("sync"));
    }
    enrichDiskCleanupSummary(m_lastCacheClearSummary, {path}, diskSuccess);
    
    refreshDataSize();
    emit lastCacheClearSummaryChanged();
    return m_lastCacheClearSummary.value(QStringLiteral("success")).toBool();
}

bool SettingsController::clearShaderVideoData()
{
    QString path = getShaderVideoPath();
    
    ThumbnailProvider* thumbnailProvider = ThumbnailProvider::instance();
    if (thumbnailProvider) {
        thumbnailProvider->clearThumbnailsByPathPrefix(path);
    }
    
    const bool diskSuccess = clearDirectory(path);
    
    if (m_sessionController) {
        m_lastCacheClearSummary = m_sessionController->pruneMediaFilesByModeAndType(
            static_cast<int>(ProcessingMode::Shader),
            static_cast<int>(MediaType::Video),
            QStringLiteral("cache-shader-video"));
        m_lastCacheClearSummary[QStringLiteral("viewerImpact")] = QStringLiteral("sync");
    } else {
        m_lastCacheClearSummary = buildCacheClearSummary(
            QStringLiteral("cache-shader-video"), true, 0, 0, 0, 0, QStringLiteral("sync"));
    }
    enrichDiskCleanupSummary(m_lastCacheClearSummary, {path}, diskSuccess);
    
    refreshDataSize();
    emit lastCacheClearSummaryChanged();
    return m_lastCacheClearSummary.value(QStringLiteral("success")).toBool();
}

bool SettingsController::clearLogs()
{
    QString path = getLogPath();
    const bool success = clearDirectory(path);
    m_lastCacheClearSummary = buildCacheClearSummary(
        QStringLiteral("cache-logs"), true, 0, 0, 0, 0, QStringLiteral("unchanged"));
    enrichDiskCleanupSummary(m_lastCacheClearSummary, {path}, success);
    refreshDataSize();
    emit lastCacheClearSummaryChanged();
    return m_lastCacheClearSummary.value(QStringLiteral("success")).toBool();
}

bool SettingsController::clearAllCache()
{
    bool success = true;
    const QString aiImagePath = getAIImagePath();
    const QString aiVideoPath = getAIVideoPath();
    const QString shaderImagePath = getShaderImagePath();
    const QString shaderVideoPath = getShaderVideoPath();
    const QString logPath = getLogPath();
    
    ThumbnailProvider* thumbnailProvider = ThumbnailProvider::instance();
    if (thumbnailProvider) {
        thumbnailProvider->clearAll();
    }

    ThumbnailDatabase* db = ThumbnailDatabase::instance();
    if (db && db->isInitialized()) {
        db->clearAll();
    }
    
    success &= clearDirectory(aiImagePath);
    success &= clearDirectory(aiVideoPath);
    success &= clearDirectory(shaderImagePath);
    success &= clearDirectory(shaderVideoPath);
    success &= clearDirectory(logPath);
    
    if (m_sessionController) {
        m_lastCacheClearSummary = m_sessionController->clearAllSessionMessagesWithStats(QStringLiteral("cache-all"));
        m_lastCacheClearSummary[QStringLiteral("viewerImpact")] = QStringLiteral("closed");
    } else {
        m_lastCacheClearSummary = buildCacheClearSummary(
            QStringLiteral("cache-all"), true, 0, 0, 0, 0, QStringLiteral("closed"));
    }
    enrichDiskCleanupSummary(
        m_lastCacheClearSummary,
        {aiImagePath, aiVideoPath, shaderImagePath, shaderVideoPath, logPath},
        success);
    
    refreshDataSize();
    emit lastCacheClearSummaryChanged();
    return m_lastCacheClearSummary.value(QStringLiteral("success")).toBool();
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
    }
    
    m_lastCacheClearSummary = buildCacheClearSummary(
        QStringLiteral("cache-thumbnails"), true, removed, 0, 0, 0, QStringLiteral("unchanged"));

    refreshDataSize();
    emit lastCacheClearSummaryChanged();
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
    m_settings->remove(QStringLiteral("behavior/previousDataPath"));
    m_settings->setValue("audio/volume", m_volume);
    m_settings->setValue("reprocess/shaderEnabled", m_autoReprocessShaderEnabled);
    m_settings->setValue("reprocess/aiEnabled", m_autoReprocessAIEnabled);
    m_settings->setValue("video/autoPlay", m_videoAutoPlay);
    m_settings->setValue("video/autoPlayOnSwitch", m_videoAutoPlayOnSwitch);
    m_settings->setValue("video/restorePosition", m_videoRestorePosition);
    m_settings->setValue("task/pauseMode", m_pauseMode);
    m_settings->sync();
}

void SettingsController::loadSettings()
{
    if (!m_settings) return;

    m_theme = m_settings->value("appearance/theme", "dark").toString();
    m_language = m_settings->value("appearance/language", "zh_CN").toString();
    m_sidebarExpanded = m_settings->value("sidebar/expanded", true).toBool();
    m_defaultSavePath = normalizeDirectoryPath(
        m_settings->value("behavior/defaultSavePath", defaultConfiguredSavePath()).toString());
    m_autoSaveResult = m_settings->value("behavior/autoSave", false).toBool();
    m_customDataPath = normalizeDirectoryPath(
        m_settings->value("behavior/customDataPath", QString()).toString());
    m_volume = m_settings->value("audio/volume", 80).toInt();
    m_autoReprocessShaderEnabled = m_settings->value("reprocess/shaderEnabled", true).toBool();
    m_autoReprocessAIEnabled = m_settings->value("reprocess/aiEnabled", true).toBool();
    m_lastExitClean = m_settings->value("system/lastExitClean", true).toBool();
    m_lastExitReason = m_settings->value("system/lastExitReason", QString()).toString();
    m_videoAutoPlay = m_settings->value("video/autoPlay", true).toBool();
    m_videoAutoPlayOnSwitch = m_settings->value("video/autoPlayOnSwitch", true).toBool();
    m_videoRestorePosition = m_settings->value("video/restorePosition", true).toBool();
    m_pauseMode = m_settings->value("task/pauseMode", 1).toInt();  // 默认模式二
    
    if (!m_settings->contains("system/prevAutoReprocessShader")) {
        m_settings->setValue("system/prevAutoReprocessShader", m_autoReprocessShaderEnabled);
    }
    if (!m_settings->contains("system/prevAutoReprocessAI")) {
        m_settings->setValue("system/prevAutoReprocessAI", m_autoReprocessAIEnabled);
    }
    m_settings->sync();
    refreshResolvedStoragePaths();
}

void SettingsController::resetToDefaults()
{
    m_theme = "dark";
    m_language = "zh_CN";
    m_sidebarExpanded = true;
    m_defaultSavePath = defaultConfiguredSavePath();
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
    m_pauseMode = 1;  // 默认模式二
    refreshResolvedStoragePaths();

    emit themeChanged();
    emit languageChanged();
    emit sidebarExpandedChanged();
    emit autoSaveResultChanged();
    emit volumeChanged();
    emit autoReprocessShaderEnabledChanged();
    emit autoReprocessAIEnabledChanged();
    emit autoReprocessAllEnabledChanged();
    emit lastExitCleanChanged();
    emit videoAutoPlayChanged();
    emit videoAutoPlayOnSwitchChanged();
    emit videoRestorePositionChanged();
    emit pauseModeChanged();
    emit customDataPathChanged();
    emit storagePathStateChanged();
    emit settingsChanged();
    saveSettings();
}

QString SettingsController::getSetting(const QString& key, const QString& defaultValue)
{
    if (!m_settings) return defaultValue;
    return m_settings->value(key, defaultValue).toString();
}

void SettingsController::setSetting(const QString& key, const QString& value)
{
    if (!m_settings) return;

    if (key == QStringLiteral("behavior/customDataPath")) {
        setCustomDataPath(value);
        return;
    }
    if (key == QStringLiteral("behavior/defaultSavePath")) {
        setDefaultSavePath(value);
        return;
    }

    m_settings->setValue(key, value);
    m_settings->sync();
    emit settingsChanged();
}

void SettingsController::setSessionController(SessionController* controller)
{
    m_sessionController = controller;
}

} // namespace EnhanceVision
