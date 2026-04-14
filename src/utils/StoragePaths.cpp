#include "EnhanceVision/utils/StoragePaths.h"

#include <QDir>
#include <QStandardPaths>

namespace EnhanceVision::StoragePaths {

QString configRootOverrideEnvVarName()
{
    return QStringLiteral("ENHANCEVISION_CONFIG_ROOT");
}

QString normalizeDirectoryPath(const QString& path)
{
    const QString trimmed = path.trimmed();
    if (trimmed.isEmpty()) {
        return {};
    }

    return QDir::cleanPath(QDir::fromNativeSeparators(trimmed));
}

QString configRootPath()
{
    const QByteArray overrideEnvVar = configRootOverrideEnvVarName().toUtf8();
    const QString overridePath =
        normalizeDirectoryPath(qEnvironmentVariable(overrideEnvVar.constData()).trimmed());
    if (!overridePath.isEmpty()) {
        return overridePath;
    }

#ifdef Q_OS_WIN
    const QString localAppData = qEnvironmentVariable("LOCALAPPDATA").trimmed();
    if (!localAppData.isEmpty()) {
        return QDir(localAppData).filePath(QStringLiteral("EnhanceVision"));
    }
#endif

    QString configPath = QStandardPaths::writableLocation(QStandardPaths::ConfigLocation);
    if (configPath.isEmpty()) {
        configPath = QDir::tempPath();
    }
    return QDir(configPath).filePath(QStringLiteral("EnhanceVision"));
}

QString settingsFilePath()
{
    return QDir(configRootPath()).filePath(QStringLiteral("settings.ini"));
}

QString defaultConfiguredDataPath()
{
    return QDir(configRootPath()).filePath(QStringLiteral("data"));
}

QString sessionsDirectoryPath()
{
    return configRootPath();
}

QString sessionsFilePath()
{
    return QDir(sessionsDirectoryPath()).filePath(QStringLiteral("sessions.json"));
}

QString uiStateFilePath()
{
    return QDir(configRootPath()).filePath(QStringLiteral("ui_state.json"));
}

QString legacySessionsFilePath()
{
    const QString appDataPath = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    return QDir(appDataPath).filePath(QStringLiteral("EnhanceVision/sessions.json"));
}

QString legacyUiStateFilePath()
{
    const QString configPath = QStandardPaths::writableLocation(QStandardPaths::ConfigLocation);
    return QDir(configPath).filePath(QStringLiteral("ui_state.json"));
}

} // namespace EnhanceVision::StoragePaths
