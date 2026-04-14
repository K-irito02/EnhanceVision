/**
 * @file InstallMaintenanceService.h
 * @brief 安装升级后的首启维护服务
 */

#ifndef ENHANCEVISION_INSTALLMAINTENANCESERVICE_H
#define ENHANCEVISION_INSTALLMAINTENANCESERVICE_H

#include <QString>

namespace EnhanceVision {

struct InstallMaintenanceIntent {
    enum class Action {
        None,
        Keep,
        Migrate,
        Delete
    };

    int version = 1;
    Action action = Action::None;
    QString previousVersion;
    QString oldDataPath;
    QString targetDataPath;
    QString legacySessionsPath;
    QString legacyUiStatePath;
    QString legacySettingsPath;

    [[nodiscard]] bool isValid() const
    {
        return action != Action::None;
    }
};

class InstallMaintenanceService
{
public:
    static QString pendingIntentFilePath();
    static bool loadIntentFile(const QString& filePath, InstallMaintenanceIntent* intent, QString* error = nullptr);
    static bool saveIntentFile(const QString& filePath, const InstallMaintenanceIntent& intent, QString* error = nullptr);
    static bool applyPendingIntent(QString* error = nullptr);
    static bool executeIntent(const InstallMaintenanceIntent& intent, QString* error = nullptr);
    static bool migrateRuntimeData(const QString& oldDataPath, const QString& newDataPath, QString* error = nullptr);

    static QString defaultLegacySessionsFilePath();
    static QString defaultLegacyUiStateFilePath();
    static QString defaultLegacySettingsFilePath();
};

} // namespace EnhanceVision

#endif // ENHANCEVISION_INSTALLMAINTENANCESERVICE_H
