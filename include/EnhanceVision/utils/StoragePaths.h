/**
 * @file StoragePaths.h
 * @brief 统一的持久化路径解析辅助
 */

#ifndef ENHANCEVISION_STORAGEPATHS_H
#define ENHANCEVISION_STORAGEPATHS_H

#include <QString>

namespace EnhanceVision::StoragePaths {

QString normalizeDirectoryPath(const QString& path);
QString configRootPath();
QString configRootOverrideEnvVarName();
QString settingsFilePath();
QString defaultConfiguredDataPath();
QString sessionsDirectoryPath();
QString sessionsFilePath();
QString uiStateFilePath();
QString legacySessionsFilePath();
QString legacyUiStateFilePath();

} // namespace EnhanceVision::StoragePaths

#endif // ENHANCEVISION_STORAGEPATHS_H
