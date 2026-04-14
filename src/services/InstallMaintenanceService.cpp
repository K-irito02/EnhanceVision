#include "EnhanceVision/services/InstallMaintenanceService.h"

#include "EnhanceVision/controllers/MessageStatusResolver.h"
#include "EnhanceVision/models/DataTypes.h"
#include "EnhanceVision/utils/StoragePaths.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSet>
#include <QThread>

#include <filesystem>

namespace EnhanceVision {

namespace {

constexpr int kIntentVersion = 1;

QString normalizePath(const QString& path)
{
    return StoragePaths::normalizeDirectoryPath(path);
}

Qt::CaseSensitivity pathCaseSensitivity()
{
#ifdef Q_OS_WIN
    return Qt::CaseInsensitive;
#else
    return Qt::CaseSensitive;
#endif
}

bool pathEquals(const QString& lhs, const QString& rhs)
{
    return QString::compare(lhs, rhs, pathCaseSensitivity()) == 0;
}

bool pathEqualsOrChildOf(const QString& candidate, const QString& base)
{
    if (candidate.isEmpty() || base.isEmpty()) {
        return false;
    }

    if (pathEquals(candidate, base)) {
        return true;
    }

    const QString normalizedBase = base.endsWith(QLatin1Char('/'))
        ? base
        : base + QLatin1Char('/');
    return candidate.startsWith(normalizedBase, pathCaseSensitivity());
}

bool ensureParentDirectory(const QString& filePath)
{
    const QFileInfo info(filePath);
    QDir dir(info.dir());
    return dir.exists() || dir.mkpath(QStringLiteral("."));
}

bool ensureDirectoryExists(const QString& path)
{
    if (path.isEmpty()) {
        return false;
    }
    QDir dir(path);
    return dir.exists() || dir.mkpath(QStringLiteral("."));
}

QString intentActionToString(InstallMaintenanceIntent::Action action)
{
    switch (action) {
    case InstallMaintenanceIntent::Action::Keep:
        return QStringLiteral("keep");
    case InstallMaintenanceIntent::Action::Migrate:
        return QStringLiteral("migrate");
    case InstallMaintenanceIntent::Action::Delete:
        return QStringLiteral("delete");
    case InstallMaintenanceIntent::Action::None:
    default:
        return QStringLiteral("none");
    }
}

InstallMaintenanceIntent::Action intentActionFromString(const QString& action)
{
    if (action == QStringLiteral("keep")) {
        return InstallMaintenanceIntent::Action::Keep;
    }
    if (action == QStringLiteral("migrate")) {
        return InstallMaintenanceIntent::Action::Migrate;
    }
    if (action == QStringLiteral("delete")) {
        return InstallMaintenanceIntent::Action::Delete;
    }
    return InstallMaintenanceIntent::Action::None;
}

QJsonObject intentToJson(const InstallMaintenanceIntent& intent)
{
    QJsonObject json;
    json[QStringLiteral("version")] = intent.version;
    json[QStringLiteral("action")] = intentActionToString(intent.action);
    json[QStringLiteral("previousVersion")] = intent.previousVersion;
    json[QStringLiteral("oldDataPath")] = intent.oldDataPath;
    json[QStringLiteral("targetDataPath")] = intent.targetDataPath;
    json[QStringLiteral("legacySessionsPath")] = intent.legacySessionsPath;
    json[QStringLiteral("legacyUiStatePath")] = intent.legacyUiStatePath;
    json[QStringLiteral("legacySettingsPath")] = intent.legacySettingsPath;
    return json;
}

InstallMaintenanceIntent intentFromJson(const QJsonObject& json)
{
    InstallMaintenanceIntent intent;
    intent.version = json.value(QStringLiteral("version")).toInt(kIntentVersion);
    intent.action = intentActionFromString(json.value(QStringLiteral("action")).toString());
    intent.previousVersion = json.value(QStringLiteral("previousVersion")).toString();
    intent.oldDataPath = normalizePath(json.value(QStringLiteral("oldDataPath")).toString());
    intent.targetDataPath = normalizePath(json.value(QStringLiteral("targetDataPath")).toString());
    intent.legacySessionsPath = normalizePath(json.value(QStringLiteral("legacySessionsPath")).toString());
    intent.legacyUiStatePath = normalizePath(json.value(QStringLiteral("legacyUiStatePath")).toString());
    intent.legacySettingsPath = normalizePath(json.value(QStringLiteral("legacySettingsPath")).toString());
    return intent;
}

bool writeJsonFile(const QString& filePath, const QJsonObject& json, QString* error = nullptr)
{
    if (!ensureParentDirectory(filePath)) {
        if (error) {
            *error = QStringLiteral("Failed to create parent directory for %1").arg(filePath);
        }
        return false;
    }

    QFile file(filePath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        if (error) {
            *error = QStringLiteral("Failed to open %1 for writing").arg(filePath);
        }
        return false;
    }

    const QByteArray data = QJsonDocument(json).toJson(QJsonDocument::Indented);
    if (file.write(data) != data.size()) {
        if (error) {
            *error = QStringLiteral("Failed to write %1").arg(filePath);
        }
        return false;
    }

    file.close();
    return true;
}

bool loadJsonFile(const QString& filePath, QJsonObject* json, QString* error = nullptr)
{
    QFile file(filePath);
    if (!file.exists()) {
        if (error) {
            *error = QStringLiteral("File does not exist: %1").arg(filePath);
        }
        return false;
    }
    if (!file.open(QIODevice::ReadOnly)) {
        if (error) {
            *error = QStringLiteral("Failed to open %1").arg(filePath);
        }
        return false;
    }

    QJsonParseError parseError;
    const QJsonDocument document = QJsonDocument::fromJson(file.readAll(), &parseError);
    if (parseError.error != QJsonParseError::NoError || !document.isObject()) {
        if (error) {
            *error = QStringLiteral("Invalid JSON in %1: %2").arg(filePath, parseError.errorString());
        }
        return false;
    }

    *json = document.object();
    return true;
}

QString resolveLegacyPath(const QString& explicitPath, const QString& fallbackPath)
{
    return explicitPath.isEmpty() ? fallbackPath : explicitPath;
}

bool removePathRecursively(const QString& path)
{
    if (path.isEmpty()) {
        return true;
    }

    const std::filesystem::path fsPath = std::filesystem::path(path.toStdWString());
    for (int attempt = 0; attempt < 5; ++attempt) {
        std::error_code removeError;
        const auto removedCount = std::filesystem::remove_all(fsPath, removeError);
        if (!removeError && (removedCount > 0 || !std::filesystem::exists(fsPath))) {
            return true;
        }
        QThread::msleep(20);
    }

    QFileInfo info(path);
    if (!info.exists()) {
        return true;
    }

    if (!info.isDir()) {
        QFile file(path);
        if (file.remove()) {
            return true;
        }

        QFile::setPermissions(path, file.permissions()
                                      | QFileDevice::WriteOwner
                                      | QFileDevice::WriteUser);
        return QFile::remove(path);
    }

    QDir dir(path);
    const QFileInfoList entries = dir.entryInfoList(
        QDir::NoDotAndDotDot | QDir::AllEntries | QDir::Hidden | QDir::System,
        QDir::DirsFirst | QDir::Name);

    bool ok = true;
    for (const QFileInfo& entry : entries) {
        ok = removePathRecursively(entry.absoluteFilePath()) && ok;
    }

    const QFileInfo dirInfo(path);
    QDir parentDir = dirInfo.dir();
    if (parentDir.exists() && !parentDir.rmdir(dirInfo.fileName())) {
        ok = false;
    }

    return ok;
}

bool directoryHasEntries(const QString& path)
{
    if (path.isEmpty()) {
        return false;
    }

    QDir dir(path);
    if (!dir.exists()) {
        return false;
    }

    return !dir.entryList(QDir::AllEntries | QDir::NoDotAndDotDot | QDir::Hidden).isEmpty();
}

QStringList legacyDataPathCandidates(const QString& preferredPath, const QString& targetPath = QString())
{
    QStringList candidates;
    const QString normalizedPreferred = normalizePath(preferredPath);
    const QString normalizedTarget = normalizePath(targetPath);

    if (!normalizedPreferred.isEmpty()) {
        candidates.append(normalizedPreferred);
    }

    const QString defaultPath = normalizePath(StoragePaths::defaultConfiguredDataPath());
    if (!defaultPath.isEmpty()) {
        candidates.append(defaultPath);
    }

    const QString legacyPath = normalizePath(QDir(StoragePaths::configRootPath())
                                                 .filePath(QStringLiteral("EnhanceVision")));
    if (!legacyPath.isEmpty()) {
        candidates.append(legacyPath);
    }

    QStringList uniqueCandidates;
    QSet<QString> seen;
    for (const QString& path : candidates) {
        if (path.isEmpty() || seen.contains(path)) {
            continue;
        }
        if (!normalizedTarget.isEmpty() && pathEquals(path, normalizedTarget)) {
            continue;
        }
        seen.insert(path);
        uniqueCandidates.append(path);
    }

    return uniqueCandidates;
}

QString resolveExistingLegacyDataPath(const QString& preferredPath, const QString& targetPath = QString())
{
    const QStringList candidates = legacyDataPathCandidates(preferredPath, targetPath);
    for (const QString& candidate : candidates) {
        if (directoryHasEntries(candidate)) {
            return candidate;
        }
    }

    for (const QString& candidate : candidates) {
        if (QFileInfo(candidate).exists()) {
            return candidate;
        }
    }

    return normalizePath(preferredPath);
}

QString uniqueConflictFilePath(const QString& destinationFilePath)
{
    QFileInfo info(destinationFilePath);
    const QString directoryPath = info.dir().absolutePath();
    const QString baseName = info.completeBaseName();
    const QString suffix = info.suffix();

    int counter = 1;
    QString candidate;
    do {
        const QString serial = QStringLiteral("legacy_%1").arg(counter++);
        candidate = suffix.isEmpty()
            ? QDir(directoryPath).filePath(QStringLiteral("%1_%2").arg(baseName, serial))
            : QDir(directoryPath).filePath(QStringLiteral("%1_%2.%3").arg(baseName, serial, suffix));
    } while (QFileInfo::exists(candidate));

    return candidate;
}

bool copyFileConflictAware(const QString& sourceFilePath, const QString& destinationFilePath, QString* error = nullptr)
{
    QFileInfo sourceInfo(sourceFilePath);
    if (!sourceInfo.exists() || !sourceInfo.isFile()) {
        return true;
    }

    if (!ensureParentDirectory(destinationFilePath)) {
        if (error) {
            *error = QStringLiteral("Failed to create destination directory for %1").arg(destinationFilePath);
        }
        return false;
    }

    QString targetPath = destinationFilePath;
    if (QFileInfo::exists(targetPath)) {
        const QFileInfo destinationInfo(targetPath);
        if (destinationInfo.size() == sourceInfo.size()) {
            return true;
        }
        targetPath = uniqueConflictFilePath(destinationFilePath);
    }

    if (!QFile::copy(sourceFilePath, targetPath)) {
        if (error) {
            *error = QStringLiteral("Failed to copy %1 to %2").arg(sourceFilePath, targetPath);
        }
        return false;
    }

    return true;
}

bool copyDirectoryRecursivelyConflictAware(const QString& sourceDirectoryPath,
                                           const QString& destinationDirectoryPath,
                                           QString* error = nullptr)
{
    QDir sourceDir(sourceDirectoryPath);
    if (!sourceDir.exists()) {
        return true;
    }

    if (!ensureDirectoryExists(destinationDirectoryPath)) {
        if (error) {
            *error = QStringLiteral("Failed to create %1").arg(destinationDirectoryPath);
        }
        return false;
    }

    const QFileInfoList entries = sourceDir.entryInfoList(
        QDir::Files | QDir::Dirs | QDir::NoDotAndDotDot | QDir::Hidden);
    for (const QFileInfo& entry : entries) {
        const QString sourcePath = entry.absoluteFilePath();
        const QString destinationPath = QDir(destinationDirectoryPath).filePath(entry.fileName());
        if (entry.isDir()) {
            if (!copyDirectoryRecursivelyConflictAware(sourcePath, destinationPath, error)) {
                return false;
            }
        } else if (!copyFileConflictAware(sourcePath, destinationPath, error)) {
            return false;
        }
    }

    return true;
}

bool moveFileToDestination(const QString& sourcePath, const QString& destinationPath, QString* error = nullptr)
{
    if (sourcePath.isEmpty() || destinationPath.isEmpty()) {
        return true;
    }

    const QString normalizedSource = normalizePath(sourcePath);
    const QString normalizedDestination = normalizePath(destinationPath);
    if (normalizedSource.isEmpty() || normalizedDestination.isEmpty() || pathEquals(normalizedSource, normalizedDestination)) {
        return true;
    }

    QFileInfo sourceInfo(normalizedSource);
    if (!sourceInfo.exists() || !sourceInfo.isFile()) {
        return true;
    }

    if (QFileInfo::exists(normalizedDestination)) {
        if (!removePathRecursively(normalizedDestination)) {
            return false;
        }
    }

    if (!ensureParentDirectory(normalizedDestination)) {
        if (error) {
            *error = QStringLiteral("Failed to create destination directory for %1").arg(normalizedDestination);
        }
        return false;
    }

    QFile sourceFile(normalizedSource);
    if (!sourceFile.open(QIODevice::ReadOnly)) {
        if (error) {
            *error = QStringLiteral("Failed to open %1 for reading").arg(normalizedSource);
        }
        return false;
    }

    const QByteArray data = sourceFile.readAll();
    sourceFile.close();

    QFile destinationFile(normalizedDestination);
    if (!destinationFile.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        if (error) {
            *error = QStringLiteral("Failed to open %1 for writing").arg(normalizedDestination);
        }
        return false;
    }

    if (destinationFile.write(data) != data.size()) {
        if (error) {
            *error = QStringLiteral("Failed to move %1 to %2").arg(normalizedSource, normalizedDestination);
        }
        destinationFile.close();
        QFile::remove(normalizedDestination);
        return false;
    }

    destinationFile.close();
    removePathRecursively(normalizedSource);
    return true;
}

MessageStatusSummary summarizeMediaStatus(const QJsonArray& mediaFiles)
{
    MessageStatusSummary summary;
    for (const QJsonValue& value : mediaFiles) {
        const auto status = static_cast<ProcessingStatus>(value.toObject().value(QStringLiteral("status")).toInt(0));
        switch (status) {
        case ProcessingStatus::Pending:
            summary.pending++;
            break;
        case ProcessingStatus::Processing:
            summary.processing++;
            break;
        case ProcessingStatus::Completed:
            summary.completed++;
            break;
        case ProcessingStatus::Failed:
            summary.failed++;
            break;
        case ProcessingStatus::Cancelled:
            summary.cancelled++;
            break;
        case ProcessingStatus::Paused:
            summary.paused++;
            break;
        case ProcessingStatus::Recoverable:
            summary.recoverable++;
            break;
        }
    }
    return summary;
}

QString remapPathPrefix(const QString& path, const QString& oldRootPath, const QString& newRootPath, bool* changed = nullptr)
{
    const QString normalizedPath = normalizePath(path);
    if (normalizedPath.isEmpty() || !pathEqualsOrChildOf(normalizedPath, oldRootPath)) {
        if (changed) {
            *changed = false;
        }
        return path;
    }

    QString relative = normalizedPath.mid(oldRootPath.size());
    if (relative.startsWith(QLatin1Char('/'))) {
        relative.remove(0, 1);
    }

    QString remapped = newRootPath;
    if (!relative.isEmpty()) {
        remapped = normalizePath(QDir(newRootPath).filePath(relative));
    }

    if (changed) {
        *changed = !pathEquals(remapped, normalizedPath);
    }
    return remapped;
}

bool remapMediaFileObject(QJsonObject& fileObject,
                          const QString& oldRootPath,
                          const QString& newRootPath,
                          int* changedPathCount)
{
    bool anyChanged = false;

    for (const QString& key : {QStringLiteral("filePath"), QStringLiteral("resultPath")}) {
        const QString currentPath = fileObject.value(key).toString();
        bool changed = false;
        const QString remapped = remapPathPrefix(currentPath, oldRootPath, newRootPath, &changed);
        if (changed) {
            fileObject.insert(key, remapped);
            anyChanged = true;
            if (changedPathCount) {
                ++(*changedPathCount);
            }
        }
    }

    const auto status = static_cast<ProcessingStatus>(fileObject.value(QStringLiteral("status")).toInt(0));
    if (status == ProcessingStatus::Failed) {
        const QString resultPath = normalizePath(fileObject.value(QStringLiteral("resultPath")).toString());
        QFileInfo resultInfo(resultPath);
        if (!resultPath.isEmpty() &&
            resultInfo.exists() &&
            resultInfo.isFile() &&
            resultInfo.size() > 0 &&
            pathEqualsOrChildOf(resultPath, newRootPath)) {
            fileObject.insert(QStringLiteral("status"), static_cast<int>(ProcessingStatus::Completed));
            anyChanged = true;
        }
    }

    return anyChanged;
}

bool remapSessionsJsonFile(const QString& filePath,
                           const QString& oldRootPath,
                           const QString& newRootPath,
                           QString* error = nullptr)
{
    if (filePath.isEmpty() || !QFileInfo::exists(filePath) || oldRootPath.isEmpty() || newRootPath.isEmpty()) {
        return true;
    }

    QJsonObject root;
    if (!loadJsonFile(filePath, &root, error)) {
        return false;
    }

    bool anyChanged = false;
    int changedPathCount = 0;
    QJsonArray sessions = root.value(QStringLiteral("sessions")).toArray();
    for (int sessionIndex = 0; sessionIndex < sessions.size(); ++sessionIndex) {
        QJsonObject sessionObject = sessions.at(sessionIndex).toObject();

        QJsonArray pendingFiles = sessionObject.value(QStringLiteral("pendingFiles")).toArray();
        for (int pendingIndex = 0; pendingIndex < pendingFiles.size(); ++pendingIndex) {
            QJsonObject pendingFile = pendingFiles.at(pendingIndex).toObject();
            if (remapMediaFileObject(pendingFile, oldRootPath, newRootPath, &changedPathCount)) {
                pendingFiles[pendingIndex] = pendingFile;
                anyChanged = true;
            }
        }
        sessionObject.insert(QStringLiteral("pendingFiles"), pendingFiles);

        QJsonArray messages = sessionObject.value(QStringLiteral("messages")).toArray();
        for (int messageIndex = 0; messageIndex < messages.size(); ++messageIndex) {
            QJsonObject messageObject = messages.at(messageIndex).toObject();
            bool messageChanged = false;

            QJsonArray mediaFiles = messageObject.value(QStringLiteral("mediaFiles")).toArray();
            for (int mediaIndex = 0; mediaIndex < mediaFiles.size(); ++mediaIndex) {
                QJsonObject mediaFile = mediaFiles.at(mediaIndex).toObject();
                if (remapMediaFileObject(mediaFile, oldRootPath, newRootPath, &changedPathCount)) {
                    mediaFiles[mediaIndex] = mediaFile;
                    messageChanged = true;
                    anyChanged = true;
                }
            }

            if (messageChanged) {
                const MessageStatusSummary summary = summarizeMediaStatus(mediaFiles);
                const ProcessingStatus status = deriveMessageStatus(summary, false);
                messageObject.insert(QStringLiteral("mediaFiles"), mediaFiles);
                messageObject.insert(QStringLiteral("status"), static_cast<int>(status));
                if (status != ProcessingStatus::Failed) {
                    messageObject.insert(QStringLiteral("errorMessage"), QString());
                }
                messages[messageIndex] = messageObject;
            }
        }

        sessionObject.insert(QStringLiteral("messages"), messages);
        sessions[sessionIndex] = sessionObject;
    }

    if (!anyChanged) {
        return true;
    }

    root.insert(QStringLiteral("sessions"), sessions);
    return writeJsonFile(filePath, root, error);
}

bool updateIniKeyValue(const QString& filePath,
                       const QString& sectionName,
                       const QString& keyName,
                       const QString& value)
{
    QStringList lines;
    QFile input(filePath);
    if (input.exists() && input.open(QIODevice::ReadOnly | QIODevice::Text)) {
        lines = QString::fromUtf8(input.readAll()).split(QLatin1Char('\n'), Qt::KeepEmptyParts);
        input.close();
    }

    const QString sectionHeader = QStringLiteral("[%1]").arg(sectionName);
    const QString keyPrefix = keyName + QLatin1Char('=');
    QStringList output;
    bool sectionFound = false;
    bool keyWritten = false;
    bool inTargetSection = false;

    for (const QString& originalLine : lines) {
        QString line = originalLine;
        if (line.endsWith(QLatin1Char('\r'))) {
            line.chop(1);
        }

        const QString trimmed = line.trimmed();
        if (trimmed.startsWith(QLatin1Char('[')) && trimmed.endsWith(QLatin1Char(']'))) {
            if (inTargetSection && !keyWritten) {
                output.append(keyPrefix + value);
                keyWritten = true;
            }

            inTargetSection = (trimmed.compare(sectionHeader, Qt::CaseInsensitive) == 0);
            sectionFound = sectionFound || inTargetSection;
            output.append(line);
            continue;
        }

        if (inTargetSection && trimmed.startsWith(keyPrefix, Qt::CaseInsensitive)) {
            output.append(keyPrefix + value);
            keyWritten = true;
            continue;
        }

        output.append(line);
    }

    if (!sectionFound) {
        if (!output.isEmpty() && !output.last().isEmpty()) {
            output.append(QString());
        }
        output.append(sectionHeader);
        output.append(keyPrefix + value);
    } else if (inTargetSection && !keyWritten) {
        output.append(keyPrefix + value);
    }

    QFile outputFile(filePath);
    if (!ensureParentDirectory(filePath) ||
        !outputFile.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text)) {
        return false;
    }

    const QByteArray encoded = output.join(QLatin1Char('\n')).toUtf8();
    if (outputFile.write(encoded) != encoded.size()) {
        return false;
    }

    outputFile.close();
    return true;
}

void updateConfiguredDataPath(const QString& configuredDataPath)
{
    updateIniKeyValue(StoragePaths::settingsFilePath(),
                      QStringLiteral("behavior"),
                      QStringLiteral("customDataPath"),
                      normalizePath(configuredDataPath));
}

bool deleteConfiguredMetadata(const QStringList& paths)
{
    bool ok = true;
    for (const QString& path : paths) {
        if (!path.isEmpty()) {
            ok = removePathRecursively(path) && ok;
        }
    }
    return ok;
}

} // namespace

QString InstallMaintenanceService::pendingIntentFilePath()
{
    return QDir(StoragePaths::configRootPath()).filePath(QStringLiteral("install_maintenance.json"));
}

bool InstallMaintenanceService::loadIntentFile(const QString& filePath,
                                               InstallMaintenanceIntent* intent,
                                               QString* error)
{
    if (!intent) {
        if (error) {
            *error = QStringLiteral("Intent output pointer is null");
        }
        return false;
    }

    QJsonObject json;
    if (!loadJsonFile(filePath, &json, error)) {
        return false;
    }

    *intent = intentFromJson(json);
    if (intent->version != kIntentVersion || !intent->isValid()) {
        if (error) {
            *error = QStringLiteral("Unsupported install maintenance intent in %1").arg(filePath);
        }
        return false;
    }

    return true;
}

bool InstallMaintenanceService::saveIntentFile(const QString& filePath,
                                               const InstallMaintenanceIntent& intent,
                                               QString* error)
{
    return writeJsonFile(filePath, intentToJson(intent), error);
}

bool InstallMaintenanceService::applyPendingIntent(QString* error)
{
    const QString intentPath = pendingIntentFilePath();
    if (!QFileInfo::exists(intentPath)) {
        return true;
    }

    InstallMaintenanceIntent intent;
    QString loadError;
    if (!loadIntentFile(intentPath, &intent, &loadError)) {
        QFile::remove(intentPath);
        if (error) {
            *error = loadError;
        }
        return false;
    }

    QString executionError;
    const bool ok = executeIntent(intent, &executionError);
    if (!ok && intent.action == InstallMaintenanceIntent::Action::Migrate && !intent.oldDataPath.isEmpty()) {
        updateConfiguredDataPath(intent.oldDataPath);
    }

    QFile::remove(intentPath);
    if (!ok && error) {
        *error = executionError;
    }
    return ok;
}

bool InstallMaintenanceService::executeIntent(const InstallMaintenanceIntent& intent, QString* error)
{
    const QString configRoot = StoragePaths::configRootPath();
    if (!ensureDirectoryExists(configRoot)) {
        if (error) {
            *error = QStringLiteral("Failed to create config root %1").arg(configRoot);
        }
        return false;
    }

    const QString sessionsPath = StoragePaths::sessionsFilePath();
    const QString uiStatePath = StoragePaths::uiStateFilePath();
    const QString legacySessionsPath = resolveLegacyPath(intent.legacySessionsPath, defaultLegacySessionsFilePath());
    const QString legacyUiStatePath = resolveLegacyPath(intent.legacyUiStatePath, defaultLegacyUiStateFilePath());
    const QString legacySettingsPath = resolveLegacyPath(intent.legacySettingsPath, defaultLegacySettingsFilePath());
    Q_UNUSED(legacySettingsPath)

    switch (intent.action) {
    case InstallMaintenanceIntent::Action::Keep: {
        if (!moveFileToDestination(legacySessionsPath, sessionsPath, error)) {
            return false;
        }
        if (!moveFileToDestination(legacyUiStatePath, uiStatePath, error)) {
            return false;
        }
        const QString keepDataPath = resolveExistingLegacyDataPath(intent.oldDataPath, intent.targetDataPath);
        if (!keepDataPath.isEmpty() && QFileInfo(keepDataPath).exists()) {
            updateConfiguredDataPath(keepDataPath);
        }
        return true;
    }
    case InstallMaintenanceIntent::Action::Migrate: {
        const QString sourceDataPath = resolveExistingLegacyDataPath(intent.oldDataPath, intent.targetDataPath);
        if (!moveFileToDestination(legacySessionsPath, sessionsPath, error)) {
            return false;
        }
        if (!moveFileToDestination(legacyUiStatePath, uiStatePath, error)) {
            return false;
        }
        if (!migrateRuntimeData(sourceDataPath, intent.targetDataPath, error)) {
            return false;
        }
        if (!remapSessionsJsonFile(sessionsPath,
                                   normalizePath(sourceDataPath),
                                   normalizePath(intent.targetDataPath),
                                   error)) {
            return false;
        }
        updateConfiguredDataPath(intent.targetDataPath);
        return true;
    }
    case InstallMaintenanceIntent::Action::Delete: {
        const QString normalizedOldDataPath = normalizePath(intent.oldDataPath);
        const QString normalizedTargetDataPath = normalizePath(intent.targetDataPath);

        if (!normalizedOldDataPath.isEmpty() && pathEquals(normalizedOldDataPath, normalizedTargetDataPath)) {
            if (!removePathRecursively(normalizedOldDataPath)) {
                if (error) {
                    *error = QStringLiteral("Failed to delete %1").arg(normalizedOldDataPath);
                }
                return false;
            }
            ensureDirectoryExists(normalizedTargetDataPath);
        } else {
            removePathRecursively(normalizedOldDataPath);
            if (!normalizedTargetDataPath.isEmpty()) {
                ensureDirectoryExists(normalizedTargetDataPath);
            }
        }

        deleteConfiguredMetadata({sessionsPath, uiStatePath, legacySessionsPath, legacyUiStatePath});

        updateConfiguredDataPath(intent.targetDataPath);
        return true;
    }
    case InstallMaintenanceIntent::Action::None:
    default:
        return true;
    }
}

bool InstallMaintenanceService::migrateRuntimeData(const QString& oldDataPath,
                                                   const QString& newDataPath,
                                                   QString* error)
{
    const QString normalizedOldDataPath = normalizePath(oldDataPath);
    const QString normalizedNewDataPath = normalizePath(newDataPath);

    if (normalizedOldDataPath.isEmpty() ||
        normalizedNewDataPath.isEmpty() ||
        pathEquals(normalizedOldDataPath, normalizedNewDataPath)) {
        return true;
    }

    QDir oldDir(normalizedOldDataPath);
    if (!oldDir.exists()) {
        return ensureDirectoryExists(normalizedNewDataPath);
    }

    std::error_code renameError;
    const std::filesystem::path sourcePath = std::filesystem::path(normalizedOldDataPath.toStdWString());
    const std::filesystem::path destinationPath = std::filesystem::path(normalizedNewDataPath.toStdWString());
    std::filesystem::rename(sourcePath, destinationPath, renameError);
    if (!renameError) {
        return true;
    }

    if (!copyDirectoryRecursivelyConflictAware(normalizedOldDataPath, normalizedNewDataPath, error)) {
        return false;
    }

    removePathRecursively(normalizedOldDataPath);

    return true;
}

QString InstallMaintenanceService::defaultLegacySessionsFilePath()
{
    return StoragePaths::legacySessionsFilePath();
}

QString InstallMaintenanceService::defaultLegacyUiStateFilePath()
{
    return StoragePaths::legacyUiStateFilePath();
}

QString InstallMaintenanceService::defaultLegacySettingsFilePath()
{
    return StoragePaths::settingsFilePath();
}

} // namespace EnhanceVision
