/**
 * @file DataBackupService.cpp
 * @brief 数据备份服务实现
 */

#include "EnhanceVision/services/DataBackupService.h"
#include "EnhanceVision/services/DatabaseService.h"
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QDateTime>
#include <QJsonDocument>
#include <QJsonObject>
#include <QDebug>

namespace EnhanceVision {

DataBackupService::DataBackupService(QObject* parent)
    : QObject(parent)
{
}

void DataBackupService::setDatabaseService(DatabaseService* db)
{
    m_dbService = db;
}

QString DataBackupService::generateBackupName() const
{
    return QDateTime::currentDateTime().toString("yyyy-MM-dd_HHmmss");
}

bool DataBackupService::copyDirectoryRecursive(const QString& src, const QString& dst,
                                               std::function<void(int)> progressCallback)
{
    QDir srcDir(src);
    if (!srcDir.exists()) return true;

    QDir(dst).mkpath(".");

    for (const QFileInfo& info : srcDir.entryInfoList(QDir::AllEntries | QDir::NoDotAndDotDot)) {
        QString destPath = dst + "/" + info.fileName();
        if (info.isDir()) {
            copyDirectoryRecursive(info.absoluteFilePath(), destPath, progressCallback);
        } else {
            QFile::copy(info.absoluteFilePath(), destPath);
        }
    }
    return true;
}

void DataBackupService::writeBackupMetadata(const QString& dir)
{
    QVariantMap meta;
    meta["timestamp"] = QDateTime::currentDateTime().toString(Qt::ISODate);
    meta["version"] = 2;

    if (m_dbService) {
        QVariantMap stats = m_dbService->getTableStats();
        meta["sessions"] = stats["sessions"];
        meta["messages"] = stats["messages"];
        meta["mediaFiles"] = stats["media_files"];
        meta["thumbnails"] = stats["thumbnails"];
    }

    QFile file(dir + "/backup_metadata.json");
    if (file.open(QIODevice::WriteOnly)) {
        file.write(QJsonDocument(QJsonObject::fromVariantMap(meta)).toJson());
        file.close();
    }
}

void DataBackupService::cleanupOldBackups()
{
    QDir backupsDir(m_dbService ? m_dbService->databasePath() : "");
    backupsDir.cdUp();

    QStringList entries = backupsDir.entryList({"backup_*"}, QDir::Dirs | QDir::NoDotAndDotDot,
                                                QDir::Name | QDir::Reversed);

    while (entries.size() > m_maxAutoBackups) {
        QString toRemove = entries.takeLast();
        QDir dir(backupsDir.filePath(toRemove));
        dir.removeRecursively();
        qInfo() << "[Backup] Cleaned up old backup:" << toRemove;
    }
}

QString DataBackupService::createFullBackup(const QString& targetDir)
{
    if (!m_dbService) {
        emit backupError("Database service not initialized");
        return QString();
    }

    QString baseDir = QFileInfo(m_dbService->databasePath()).absolutePath();
    QString backupDir = targetDir.isEmpty()
        ? baseDir + "/backups/backup_" + generateBackupName()
        : targetDir;

    QDir().mkpath(backupDir);

    emit backupProgress(0, "preparing");

    struct CopyTask { QString src; QString dst; QString label; };
    QVector<CopyTask> tasks = {
        { m_dbService->databasePath(), backupDir + "/enhancevision.db", "database" },
        { baseDir + "/ai_images", backupDir + "/ai_images", "ai_images" },
        { baseDir + "/ai_videos", backupDir + "/ai_videos", "ai_videos" },
        { baseDir + "/shader_images", backupDir + "/shader_images", "shader_images" },
        { baseDir + "/shader_videos", backupDir + "/shader_videos", "shader_videos" },
        { baseDir + "/logs", backupDir + "/logs", "logs" },
    };

    int perTask = 90 / tasks.size();
    for (int i = 0; i < tasks.size(); ++i) {
        emit backupProgress(5 + perTask * i, tasks[i].label);
        copyDirectoryRecursive(tasks[i].src, tasks[i].dst, [](int){});
    }

    emit backupProgress(95, "metadata");
    writeBackupMetadata(backupDir);

    emit backupProgress(100, "done");
    cleanupOldBackups();

    emit backupCompleted(backupDir, true);
    return backupDir;
}

QString DataBackupService::createQuickBackup()
{
    if (!m_dbService) return QString();

    QString baseDir = QFileInfo(m_dbService->databasePath()).absolutePath();
    QString backupDir = baseDir + "/backups/quick_" + generateBackupName();
    QDir().mkpath(backupDir);

    QFile::copy(m_dbService->databasePath(), backupDir + "/enhancevision.db");
    writeBackupMetadata(backupDir);

    emit backupCompleted(backupDir, true);
    return backupDir;
}

QVariantList DataBackupService::getBackupList()
{
    QVariantList result;
    QString baseDir = m_dbService ? QFileInfo(m_dbService->databasePath()).absolutePath() : "";
    QDir dir(baseDir + "/backups");

    for (const QFileInfo& info : dir.entryInfoList(
             {"backup_*", "quick_*"}, QDir::Dirs | QDir::NoDotAndDotDot, QDir::Name | QDir::Reversed)) {

        QVariantMap item;
        item["path"] = info.absoluteFilePath();
        item["name"] = info.fileName();
        item["size"] = 0;

        QString metaFile = info.absoluteFilePath() + "/backup_metadata.json";
        QFile mf(metaFile);
        if (mf.open(QIODevice::ReadOnly)) {
            QJsonDocument doc = QJsonDocument::fromJson(mf.readAll());
            mf.close();
            item["timestamp"] = doc.object()["timestamp"].toString();
            item["version"] = doc.object()["version"].toInt(0);
        } else {
            item["timestamp"] = info.lastModified().toString(Qt::ISODate);
        }

        result.append(item);
    }

    return result;
}

bool DataBackupService::deleteBackup(const QString& backupPath)
{
    QDir dir(backupPath);
    bool ok = dir.removeRecursively();
    if (!ok) {
        emit backupError("Failed to delete backup: " + backupPath);
    }
    return ok;
}

bool DataBackupService::exportAsZip(const QString& targetPath)
{
    createFullBackup(targetPath);
    // TODO: 实现压缩为 ZIP（需要额外依赖如 QuaZip）
    return false;
}

void DataBackupService::setAutoBackupPolicy(int intervalHours, int maxBackups)
{
    m_autoBackupIntervalHours = intervalHours;
    m_maxAutoBackups = maxBackups;

    if (!m_autoBackupTimer) {
        m_autoBackupTimer = new QTimer(this);
        connect(m_autoBackupTimer, &QTimer::timeout, this, [this]() {
            createQuickBackup();
        });
    }

    if (intervalHours > 0) {
        m_autoBackupTimer->start(intervalHours * 3600 * 1000);
    } else {
        m_autoBackupTimer->stop();
    }
}

} // namespace EnhanceVision
