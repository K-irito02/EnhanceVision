/**
 * @file DataRestoreService.cpp
 * @brief 数据恢复服务实现
 */

#include "EnhanceVision/services/DataRestoreService.h"
#include "EnhanceVision/services/DatabaseService.h"
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>

namespace EnhanceVision {

DataRestoreService::DataRestoreService(QObject* parent)
    : QObject(parent)
{
}

void DataRestoreService::setDatabaseService(DatabaseService* db)
{
    m_dbService = db;
}

bool DataRestoreService::copyDirectoryRecursive(const QString& src, const QString& dst,
                                                 std::function<void(int)> progressCallback)
{
    QDir srcDir(src);
    if (!srcDir.exists()) return false;

    QDir(dst).mkpath(".");

    int totalFiles = 0;
    int copiedFiles = 0;

    for (const QFileInfo& info : srcDir.entryInfoList(QDir::AllEntries | QDir::NoDotAndDotDot)) {
        if (info.isFile()) totalFiles++;
    }

    for (const QFileInfo& info : srcDir.entryInfoList(QDir::AllEntries | QDir::NoDotAndDotDot)) {
        QString destPath = dst + "/" + info.fileName();
        if (info.isDir()) {
            copyDirectoryRecursive(info.absoluteFilePath(), destPath, progressCallback);
        } else {
            QFile::copy(info.absoluteFilePath(), destPath);
            copiedFiles++;
            if (progressCallback && totalFiles > 0) {
                progressCallback(copiedFiles * 100 / totalFiles);
            }
        }
    }
    return true;
}

QVariantMap DataRestoreService::analyzeDataDirectory(const QString& path)
{
    QVariantMap result;
    result["valid"] = false;
    result["hasDatabase"] = false;
    result["sessions"] = 0;
    result["messages"] = 0;
    result["mediaFiles"] = 0;
    result["thumbnails"] = 0;

    QDir dir(path);

    if (!dir.exists()) {
        return result;
    }

    bool hasDb = dir.exists("enhancevision.db");
    result["valid"] = hasDb || !dir.isEmpty();
    result["hasDatabase"] = hasDb;

    if (hasDb) {
        QJsonDocument metaDoc;
        QString metaFile = path + "/backup_metadata.json";
        QFile mf(metaFile);
        if (mf.open(QIODevice::ReadOnly)) {
            metaDoc = QJsonDocument::fromJson(mf.readAll());
            mf.close();
        }
        QJsonObject meta = metaDoc.object();
        result["sessions"] = meta["sessions"].toInt(0);
        result["messages"] = meta["messages"].toInt(0);
        result["mediaFiles"] = meta["mediaFiles"].toInt(0);
        result["thumbnails"] = meta["thumbnails"].toInt(0);
    }

    QStringList subdirs = {"ai_images", "ai_videos", "shader_images", "shader_videos", "logs"};
    for (const QString& subdir : subdirs) {
        if (dir.exists(subdir)) {
            int count = dir.entryList({subdir}, QDir::Dirs).size();
            result[subdir + "_count"] = count;
        }
    }

    return result;
}

QVariantMap DataRestoreService::validateBackupSource(const QString& path)
{
    QVariantMap analysis = analyzeDataDirectory(path);
    if (!analysis["valid"].toBool()) {
        analysis["error"] = tr("Selected directory does not contain valid backup data");
    }
    return analysis;
}

QVariantMap DataRestoreService::getRestorePreview(const QString& path)
{
    return analyzeDataDirectory(path);
}

bool DataRestoreService::restoreFromPath(const QString& sourcePath)
{
    if (!m_dbService) {
        emit restoreError("Database service not initialized");
        return false;
    }

    QVariantMap validation = validateBackupSource(sourcePath);
    if (!validation["valid"].toBool()) {
        emit restoreError(validation["error"].toString());
        return false;
    }

    emit restoreProgress(0, "preparing");

    QString baseDir = QFileInfo(m_dbService->databasePath()).absolutePath();

    QString preRestoreBackup = baseDir + "/backups/pre_restore_" +
                               QDateTime::currentDateTime().toString("yyyy-MM-dd_HHmmss");

    copyDirectoryRecursive(baseDir, preRestoreBackup, [](int){});
    qInfo() << "[Restore] Pre-restore backup created:" << preRestoreBackup;

    m_dbService->db().close();

    struct CopyTask { QString src; QString dst; QString label; };
    QVector<CopyTask> tasks = {
        { sourcePath + "/enhancevision.db", baseDir + "/enhancevision.db", "database" },
        { sourcePath + "/ai_images", baseDir + "/ai_images", "ai_images" },
        { sourcePath + "/ai_videos", baseDir + "/ai_videos", "ai_videos" },
        { sourcePath + "/shader_images", baseDir + "/shader_images", "shader_images" },
        { sourcePath + "/shader_videos", baseDir + "/shader_videos", "shader_videos" },
        { sourcePath + "/logs", baseDir + "/logs", "logs" },
    };

    int perTask = 90 / tasks.size();
    for (int i = 0; i < tasks.size(); ++i) {
        emit restoreProgress(5 + perTask * i, tasks[i].label);
        copyDirectoryRecursive(tasks[i].src, tasks[i].dst, [this, i, perTask](int p) {
            emit restoreProgress(5 + perTask * i + p * perTask / 100, "");
        });
    }

    emit restoreProgress(95, "reopening_database");

    m_dbService->db().open();
    m_dbService->checkAndMigrate();

    emit restoreProgress(100, "done");
    emit restoreCompleted(true, tr("Data restored successfully from: %1").arg(sourcePath));
    return true;
}

} // namespace EnhanceVision
