/**
 * @file DataRestoreService.h
 * @brief 数据恢复服务
 * @author Qt客户端开发工程师
 */

#ifndef ENHANCEVISION_DATARESTORESERVICE_H
#define ENHANCEVISION_DATARESTORESERVICE_H

#include <QObject>
#include <QString>
#include <QVariantMap>

namespace EnhanceVision {

class DatabaseService;

class DataRestoreService : public QObject
{
    Q_OBJECT

public:
    explicit DataRestoreService(QObject* parent = nullptr);
    ~DataRestoreService() override = default;

    void setDatabaseService(DatabaseService* db);

    Q_INVOKABLE QVariantMap validateBackupSource(const QString& path);
    Q_INVOKABLE bool restoreFromPath(const QString& sourcePath);
    Q_INVOKABLE QVariantMap getRestorePreview(const QString& path);

signals:
    void restoreProgress(int percent, const QString& stage);
    void restoreCompleted(bool success, const QString& message);
    void restoreError(const QString& error);

private:
    bool copyDirectoryRecursive(const QString& src, const QString& dst,
                                std::function<void(int)> progressCallback);
    QVariantMap analyzeDataDirectory(const QString& path);
    DatabaseService* m_dbService = nullptr;
};

} // namespace EnhanceVision

#endif // ENHANCEVISION_DATARESTORESERVICE_H
