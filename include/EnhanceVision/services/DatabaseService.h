/**
 * @file DatabaseService.h
 * @brief 数据库核心服务 - 连接管理、CRUD、事务、迁移
 * @author Qt客户端开发工程师
 */

#ifndef ENHANCEVISION_DATABACESERVICE_H
#define ENHANCEVISION_DATABACESERVICE_H

#include <QObject>
#include <QString>
#include <QSqlDatabase>
#include <QImage>
#include <QVariantMap>
#include <QVariantList>
#include <QList>
#include <QThread>
#include "EnhanceVision/models/DataTypes.h"

namespace EnhanceVision {

class Session;
class Message;

class DatabaseService : public QObject
{
    Q_OBJECT

public:
    static DatabaseService* instance();
    static void destroyInstance();

    bool initialize();
    bool isInitialized() const { return m_initialized; }

    QSqlDatabase db() const { return m_db; }
    QString databasePath() const { return m_dbPath; }

    QSqlDatabase threadDb() const;

    Q_INVOKABLE qint64 databaseFileSize() const;

    // ===== 事务 =====
    bool beginTransaction();
    bool commitTransaction();
    bool rollbackTransaction();

    // ===== 元数据 =====
    QString getMetadata(const QString& key, const QString& defaultValue = QString()) const;
    int getMetadataInt(const QString& key, int defaultValue = 0) const;
    bool setMetadata(const QString& key, const QString& value);
    bool setMetadataInt(const QString& key, int value);

    // ===== Sessions CRUD =====
    bool insertSession(const Session& session);
    bool updateSession(const Session& session);
    bool deleteSession(const QString& sessionId);
    QList<Session> loadAllSessions();
    QList<Message> loadSessionMessages(const QString& sessionId);
    void updateSessionCounter(int counter);

    // ===== Messages CRUD =====
    bool insertMessage(const QString& sessionId, const Message& message);
    bool updateMessage(const Message& message);
    bool deleteMessage(const QString& messageId);
    bool updateMessageProgress(const QString& messageId, int progress);
    bool updateMessageStatus(const QString& messageId, int status);

    // ===== MediaFiles CRUD =====
    QString insertMediaFile(const QString& messageId, const MediaFile& mediaFile);
    bool updateMediaFile(const MediaFile& mediaFile);
    bool deleteMediaFile(const QString& fileId);
    QList<MediaFile> loadMessageMediaFiles(const QString& messageId);
    int getMessageMediaFileCount(const QString& messageId) const;

    // ===== PendingFiles CRUD =====
    bool insertPendingFile(const QString& sessionId, const MediaFile& pendingFile);
    QList<MediaFile> loadPendingFiles(const QString& sessionId);
    bool clearPendingFiles(const QString& sessionId);
    bool deletePendingFile(const QString& pendingFileId);

    // ===== Thumbnails CRUD =====
    bool saveThumbnail(const QString& mediaFileId, const QImage& thumbnail);
    QImage loadThumbnail(const QString& mediaFileId) const;
    bool deleteThumbnail(const QString& mediaFileId);
    bool thumbnailExists(const QString& mediaFileId) const;
    int thumbnailCount() const;
    qint64 totalThumbnailSize() const;

    // ===== 批量操作 =====
    bool saveFullSession(const Session& session, const QList<Message>& messages);

    // ===== 会话级清理（级联） =====
    bool clearSessionData(const QString& sessionId);
    bool deleteSessions(const QStringList& sessionIds);

    // ===== 按条件清理 =====
    int clearAllMessagesByMode(int mode);
    int clearAllShaderVideoMessages();
    int clearMediaFilesByModeAndType(int mode, int mediaType);
    int clearAllMessages();
    bool deleteAllData();

    // ===== 缩略图专项清理 =====
    int clearThumbnailsByPathPrefix(const QString& pathPrefix);
    int clearAllThumbnails();
    int cleanupOrphanedThumbnails();

    // ===== 统计查询 =====
    QVariantMap getTableStats() const;

    // ===== 迁移相关 =====
    void checkAndMigrate();
    bool migrateV1toV2(const QString& jsonFilePath);
    bool createBackupBeforeMigration(int currentVersion);
    bool restoreFromBackup();

private:
    explicit DatabaseService(QObject* parent = nullptr);
    ~DatabaseService() override;

    DatabaseService(const DatabaseService&) = delete;
    DatabaseService& operator=(const DatabaseService&) = delete;

    static DatabaseService* s_instance;

    QString resolveDatabasePath() const;
    bool createTables();
    bool execPragma(const QString& pragma);
    bool execSql(const QString& sql);
    bool execBatch(const QStringList& sqls);

    Session rowToSession(const QSqlQuery& query) const;
    Message rowToMessage(const QSqlQuery& query) const;
    MediaFile rowToMediaFile(const QSqlQuery& query) const;

    QVariantMap messageToParamsMap(const Message& msg) const;
    QMap<QString, QVariant> shaderParamsToMap(const ShaderParams& params) const;
    ShaderParams mapToShaderParams(const QVariantMap& map) const;

    bool runMigrationStep(int fromVersion, int toVersion);

    QSqlDatabase m_db;
    QString m_dbPath;
    bool m_initialized = false;
    static constexpr int CURRENT_SCHEMA_VERSION = 2;
};

} // namespace EnhanceVision

#endif // ENHANCEVISION_DATABACESERVICE_H
