/**
 * @file DataBackupService.h
 * @brief 数据备份服务
 * @author Qt客户端开发工程师
 */

#ifndef ENHANCEVISION_DATABACKUPSERVICE_H
#define ENHANCEVISION_DATABACKUPSERVICE_H

#include <QObject>
#include <QString>
#include <QVariantList>
#include <QTimer>

namespace EnhanceVision {

class DatabaseService;

class DataBackupService : public QObject
{
    Q_OBJECT

public:
    explicit DataBackupService(QObject* parent = nullptr);
    ~DataBackupService() override = default;

    void setDatabaseService(DatabaseService* db);

    Q_INVOKABLE QString createFullBackup(const QString& targetDir = QString());
    Q_INVOKABLE QString createQuickBackup();
    Q_INVOKABLE QVariantList getBackupList();
    Q_INVOKABLE bool deleteBackup(const QString& backupPath);
    Q_INVOKABLE void setAutoBackupPolicy(int intervalHours, int maxBackups = 10);
    Q_INVOKABLE bool exportAsZip(const QString& targetPath);

signals:
    void backupProgress(int percent, const QString& stage);
    void backupCompleted(const QString& backupPath, bool success);
    void backupError(const QString& error);

private:
    QString generateBackupName() const;
    bool copyDirectoryRecursive(const QString& src, const QString& dst,
                                std::function<void(int)> progressCallback);
    void writeBackupMetadata(const QString& dir);
    void cleanupOldBackups();

    DatabaseService* m_dbService = nullptr;
    QTimer* m_autoBackupTimer = nullptr;
    int m_autoBackupIntervalHours = 0;
    int m_maxAutoBackups = 10;
};

} // namespace EnhanceVision

#endif // ENHANCEVISION_DATABACKUPSERVICE_H
