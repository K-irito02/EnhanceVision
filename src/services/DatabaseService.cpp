/**
 * @file DatabaseService.cpp
 * @brief 数据库核心服务实现
 */

#include "EnhanceVision/services/DatabaseService.h"
#include <QSqlQuery>
#include <QSqlError>
#include <QSqlRecord>
#include <QBuffer>
#include <QByteArray>
#include <QDir>
#include <QFileInfo>
#include <QStandardPaths>
#include <QDateTime>
#include <QDebug>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QThread>

namespace EnhanceVision {

DatabaseService* DatabaseService::s_instance = nullptr;

DatabaseService::DatabaseService(QObject* parent)
    : QObject(parent)
{
}

DatabaseService::~DatabaseService()
{
    if (m_db.isOpen()) {
        m_db.close();
    }
}

DatabaseService* DatabaseService::instance()
{
    if (!s_instance) {
        s_instance = new DatabaseService();
    }
    return s_instance;
}

void DatabaseService::destroyInstance()
{
    delete s_instance;
    s_instance = nullptr;
}

QString DatabaseService::resolveDatabasePath() const
{
    QString baseDir;
    if (baseDir.isEmpty()) {
        baseDir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    }
    QDir dir(baseDir);
    if (!dir.exists()) {
        dir.mkpath(".");
    }
    return dir.filePath("enhancevision.db");
}

bool DatabaseService::execPragma(const QString& pragma)
{
    QSqlQuery query(threadDb());
    if (!query.exec(pragma)) {
        qWarning() << "[DB] Pragma failed:" << pragma << query.lastError().text();
        return false;
    }
    return true;
}

bool DatabaseService::execSql(const QString& sql)
{
    QSqlQuery query(threadDb());
    if (!query.exec(sql)) {
        qWarning() << "[DB] SQL failed:" << sql.left(100) << query.lastError().text();
        return false;
    }
    return true;
}

QSqlDatabase DatabaseService::threadDb() const
{
    const QString baseConn = "enhancevision_conn";
    const QString threadConn = baseConn + "_" + QString::number(reinterpret_cast<quintptr>(QThread::currentThreadId()));

    if (!QSqlDatabase::contains(threadConn)) {
        QSqlDatabase db = QSqlDatabase::addDatabase("QSQLITE", threadConn);
        db.setDatabaseName(m_dbPath);
        if (!db.open()) {
            qCritical() << "[DB] Failed to open thread connection:" << threadConn
                       << "Error:" << db.lastError().text();
            return m_db;
        }
        db.exec("PRAGMA foreign_keys = ON");
        db.exec("PRAGMA synchronous = NORMAL");
    }
    return QSqlDatabase::database(threadConn);
}

bool DatabaseService::execBatch(const QStringList& sqls)
{
    for (const QString& sql : sqls) {
        if (!execSql(sql)) {
            return false;
        }
    }
    return true;
}

bool DatabaseService::initialize()
{
    m_dbPath = resolveDatabasePath();

    m_db = QSqlDatabase::addDatabase("QSQLITE", "enhancevision_conn");
    m_db.setDatabaseName(m_dbPath);

    if (!m_db.open()) {
        qCritical() << "[DB] Failed to open database:" << m_dbPath
                   << "Error:" << m_db.lastError().text();
        return false;
    }

    QSqlDatabase db = threadDb();
    execPragma("PRAGMA foreign_keys = ON");
    execPragma("PRAGMA journal_mode = WAL");
    execPragma("PRAGMA synchronous = NORMAL");
    execPragma("PRAGMA cache_size = -4096");

    if (!createTables()) {
        qCritical() << "[DB] Failed to create tables";
        return false;
    }

    checkAndMigrate();

    m_initialized = true;
    qInfo() << "[DB] Database initialized:" << m_dbPath;
    return true;
}

bool DatabaseService::createTables()
{
    QStringList sqls = {
        R"(
CREATE TABLE IF NOT EXISTS sessions (
    id              TEXT PRIMARY KEY,
    name            TEXT NOT NULL DEFAULT '',
    created_at      INTEGER NOT NULL DEFAULT (strftime('%s','now')),
    modified_at     INTEGER NOT NULL DEFAULT (strftime('%s','now')),
    is_active       INTEGER NOT NULL DEFAULT 0,
    is_selected     INTEGER NOT NULL DEFAULT 0,
    is_pinned       INTEGER NOT NULL DEFAULT 0,
    sort_index      INTEGER NOT NULL DEFAULT 0
))",

        R"(CREATE INDEX IF NOT EXISTS idx_sessions_sort ON sessions(is_pinned DESC, sort_index ASC))",
        R"(CREATE INDEX IF NOT EXISTS idx_sessions_modified ON sessions(modified_at DESC))",

        R"(
CREATE TABLE IF NOT EXISTS messages (
    id                  TEXT PRIMARY KEY,
    session_id          TEXT NOT NULL REFERENCES sessions(id) ON DELETE CASCADE,
    timestamp           INTEGER NOT NULL DEFAULT (strftime('%s','now')),
    mode                INTEGER NOT NULL DEFAULT 0,
    status              INTEGER NOT NULL DEFAULT 0,
    progress            INTEGER NOT NULL DEFAULT 0,
    queue_position      INTEGER NOT NULL DEFAULT -1,
    error_message       TEXT DEFAULT '',
    parameters          TEXT DEFAULT '{}',
    shader_params       TEXT DEFAULT '{}',
    ai_model_id         TEXT DEFAULT '',
    ai_category         INTEGER NOT NULL DEFAULT 0,
    ai_use_gpu          INTEGER NOT NULL DEFAULT 1,
    ai_tile_size        INTEGER NOT NULL DEFAULT 0,
    ai_auto_tile_size   INTEGER NOT NULL DEFAULT 1,
    ai_model_params     TEXT DEFAULT '{}'
))",

        R"(CREATE INDEX IF NOT EXISTS idx_messages_session ON messages(session_id))",
        R"(CREATE INDEX IF NOT EXISTS idx_messages_status ON messages(status))",
        R"(CREATE INDEX IF NOT EXISTS idx_messages_mode ON messages(mode))",
        R"(CREATE INDEX IF NOT EXISTS idx_messages_timestamp ON messages(timestamp DESC))",

        R"(
CREATE TABLE IF NOT EXISTS media_files (
    id              TEXT PRIMARY KEY,
    message_id      TEXT REFERENCES messages(id) ON DELETE CASCADE,
    pending_file_id TEXT REFERENCES pending_files(id) ON DELETE SET NULL,
    file_path       TEXT NOT NULL DEFAULT '',
    original_path   TEXT DEFAULT '',
    file_name       TEXT NOT NULL DEFAULT '',
    file_size       INTEGER NOT NULL DEFAULT 0,
    media_type      INTEGER NOT NULL DEFAULT 0,
    duration        INTEGER NOT NULL DEFAULT 0,
    resolution_w    INTEGER NOT NULL DEFAULT 0,
    resolution_h    INTEGER NOT NULL DEFAULT 0,
    status          INTEGER NOT NULL DEFAULT 0,
    result_path     TEXT DEFAULT ''
))",

        R"(CREATE INDEX IF NOT EXISTS idx_media_files_message ON media_files(message_id))",
        R"(CREATE INDEX IF NOT EXISTS idx_media_files_pending ON media_files(pending_file_id))",
        R"(CREATE INDEX IF NOT EXISTS idx_media_files_status ON media_files(status))",

        R"(
CREATE TABLE IF NOT EXISTS pending_files (
    id          TEXT PRIMARY KEY,
    session_id  TEXT NOT NULL REFERENCES sessions(id) ON DELETE CASCADE,
    file_path   TEXT NOT NULL DEFAULT '',
    file_name   TEXT NOT NULL DEFAULT '',
    file_size   INTEGER NOT NULL DEFAULT 0,
    media_type  INTEGER NOT NULL DEFAULT 0,
    duration    INTEGER NOT NULL DEFAULT 0,
    width       INTEGER NOT NULL DEFAULT 0,
    height      INTEGER NOT NULL DEFAULT 0,
    status      INTEGER NOT NULL DEFAULT 0,
    result_path TEXT DEFAULT ''
))",
        R"(CREATE INDEX IF NOT EXISTS idx_pending_files_session ON pending_files(session_id))",

        R"(
CREATE TABLE IF NOT EXISTS thumbnails (
    media_file_id   TEXT PRIMARY KEY REFERENCES media_files(id) ON DELETE CASCADE,
    thumbnail       BLOB,
    width           INTEGER NOT NULL DEFAULT 256,
    height          INTEGER NOT NULL DEFAULT 256,
    updated_at      INTEGER NOT NULL DEFAULT (strftime('%s','now'))
))",

        R"(
CREATE TABLE IF NOT EXISTS app_metadata (
    key   TEXT PRIMARY KEY,
    value TEXT NOT NULL
))"
    };

    bool ok = execBatch(sqls);
    if (ok) {
        execSql(R"(INSERT OR IGNORE INTO app_metadata(key, value) VALUES ('version', '2'))");
        execSql(R"(INSERT OR IGNORE INTO app_metadata(key, value) VALUES ('session_counter', '0'))");
        execSql(R"(INSERT OR IGNORE INTO app_metadata(key, value) VALUES ('last_active_session_id', ''))");
    }

    return ok;
}

// ===== 元数据 =====

QString DatabaseService::getMetadata(const QString& key, const QString& defaultValue) const
{
    QSqlQuery query(threadDb());
    query.prepare("SELECT value FROM app_metadata WHERE key = ?");
    query.addBindValue(key);
    if (query.exec() && query.next()) {
        return query.value(0).toString();
    }
    return defaultValue;
}

int DatabaseService::getMetadataInt(const QString& key, int defaultValue) const
{
    bool ok = false;
    int val = getMetadata(key).toInt(&ok);
    return ok ? val : defaultValue;
}

bool DatabaseService::setMetadata(const QString& key, const QString& value)
{
    QSqlQuery query(threadDb());
    query.prepare("INSERT OR REPLACE INTO app_metadata(key, value) VALUES (?, ?)");
    query.addBindValue(key);
    query.addBindValue(value);
    if (!query.exec()) {
        qWarning() << "[DB] setMetadata failed:" << key << query.lastError().text();
        return false;
    }
    return true;
}

bool DatabaseService::setMetadataInt(const QString& key, int value)
{
    return setMetadata(key, QString::number(value));
}

// ===== 事务 =====

bool DatabaseService::beginTransaction()
{
    return execSql("BEGIN IMMEDIATE TRANSACTION");
}

bool DatabaseService::commitTransaction()
{
    return execSql("COMMIT");
}

bool DatabaseService::rollbackTransaction()
{
    return execSql("ROLLBACK");
}

// ===== 数据转换辅助方法 =====

Session DatabaseService::rowToSession(const QSqlQuery& query) const
{
    Session s;
    s.id = query.value("id").toString();
    s.name = query.value("name").toString();
    s.createdAt = QDateTime::fromSecsSinceEpoch(query.value("created_at").toLongLong());
    s.modifiedAt = QDateTime::fromSecsSinceEpoch(query.value("modified_at").toLongLong());
    s.isActive = query.value("is_active").toBool();
    s.isSelected = query.value("is_selected").toBool();
    s.isPinned = query.value("is_pinned").toBool();
    s.sortIndex = query.value("sort_index").toInt();
    return s;
}

Message DatabaseService::rowToMessage(const QSqlQuery& query) const
{
    Message msg;
    msg.id = query.value("id").toString();
    msg.timestamp = QDateTime::fromSecsSinceEpoch(query.value("timestamp").toLongLong());
    msg.mode = static_cast<ProcessingMode>(query.value("mode").toInt());
    msg.status = static_cast<ProcessingStatus>(query.value("status").toInt());
    msg.progress = query.value("progress").toInt();
    msg.queuePosition = query.value("queue_position").toInt();
    msg.errorMessage = query.value("error_message").toString();

    QJsonDocument paramsDoc = QJsonDocument::fromJson(query.value("parameters").toString().toUtf8());
    msg.parameters = paramsDoc.object().toVariantMap();

    QJsonDocument shaderDoc = QJsonDocument::fromJson(query.value("shader_params").toString().toUtf8());
    msg.shaderParams = mapToShaderParams(shaderDoc.object().toVariantMap());

    msg.aiParams.modelId = query.value("ai_model_id").toString();
    msg.aiParams.category = static_cast<ModelCategory>(query.value("ai_category").toInt());
    msg.aiParams.useGpu = query.value("ai_use_gpu").toBool();
    msg.aiParams.tileSize = query.value("ai_tile_size").toInt();

    QJsonDocument aiParamsDoc = QJsonDocument::fromJson(query.value("ai_model_params").toString().toUtf8());
    msg.aiParams.modelParams = aiParamsDoc.object().toVariantMap();

    return msg;
}

MediaFile DatabaseService::rowToMediaFile(const QSqlQuery& query) const
{
    MediaFile f;
    f.id = query.value("id").toString();
    f.filePath = query.value("file_path").toString();
    f.originalPath = query.value("original_path").toString();
    f.fileName = query.value("file_name").toString();
    f.fileSize = query.value("file_size").toLongLong();
    f.type = static_cast<MediaType>(query.value("media_type").toInt());
    f.duration = query.value("duration").toLongLong();
    f.resolution = QSize(query.value("resolution_w").toInt(), query.value("resolution_h").toInt());
    f.status = static_cast<ProcessingStatus>(query.value("status").toInt());
    f.resultPath = query.value("result_path").toString();
    return f;
}

QMap<QString, QVariant> DatabaseService::shaderParamsToMap(const ShaderParams& params) const
{
    QMap<QString, QVariant> m;
    m["brightness"] = params.brightness;
    m["contrast"] = params.contrast;
    m["saturation"] = params.saturation;
    m["sharpness"] = params.sharpness;
    m["blur"] = params.blur;
    m["denoise"] = params.denoise;
    m["hue"] = params.hue;
    m["exposure"] = params.exposure;
    m["gamma"] = params.gamma;
    m["temperature"] = params.temperature;
    m["tint"] = params.tint;
    m["vignette"] = params.vignette;
    m["highlights"] = params.highlights;
    m["shadows"] = params.shadows;
    return m;
}

ShaderParams DatabaseService::mapToShaderParams(const QVariantMap& map) const
{
    ShaderParams p;
    p.brightness = map.value("brightness", 0.0).toDouble();
    p.contrast = map.value("contrast", 1.0).toDouble();
    p.saturation = map.value("saturation", 1.0).toDouble();
    p.sharpness = map.value("sharpness", 0.0).toDouble();
    p.blur = map.value("blur", 0.0).toDouble();
    p.denoise = map.value("denoise", 0.0).toDouble();
    p.hue = map.value("hue", 0.0).toDouble();
    p.exposure = map.value("exposure", 0.0).toDouble();
    p.gamma = map.value("gamma", 1.0).toDouble();
    p.temperature = map.value("temperature", 0.0).toDouble();
    p.tint = map.value("tint", 0.0).toDouble();
    p.vignette = map.value("vignette", 0.0).toDouble();
    p.highlights = map.value("highlights", 0.0).toDouble();
    p.shadows = map.value("shadows", 0.0).toDouble();
    return p;
}

// ===== Sessions CRUD =====

bool DatabaseService::insertSession(const Session& session)
{
    QSqlQuery query(threadDb());
    query.prepare(R"(
        INSERT OR REPLACE INTO sessions (id, name, created_at, modified_at, is_active, is_selected, is_pinned, sort_index)
        VALUES (?, ?, ?, ?, ?, ?, ?, ?)
    )");
    query.addBindValue(session.id);
    query.addBindValue(session.name);
    query.addBindValue(static_cast<qint64>(session.createdAt.toSecsSinceEpoch()));
    query.addBindValue(static_cast<qint64>(session.modifiedAt.toSecsSinceEpoch()));
    query.addBindValue(session.isActive ? 1 : 0);
    query.addBindValue(session.isSelected ? 1 : 0);
    query.addBindValue(session.isPinned ? 1 : 0);
    query.addBindValue(session.sortIndex);

    if (!query.exec()) {
        qWarning() << "[DB] insertSession failed:" << query.lastError().text();
        return false;
    }
    return true;
}

bool DatabaseService::updateSession(const Session& session)
{
    return insertSession(session);
}

bool DatabaseService::deleteSession(const QString& sessionId)
{
    QSqlQuery query(threadDb());
    query.prepare("DELETE FROM sessions WHERE id = ?");
    query.addBindValue(sessionId);
    if (!query.exec()) {
        qWarning() << "[DB] deleteSession failed:" << query.lastError().text();
        return false;
    }
    return true;
}

QList<Session> DatabaseService::loadAllSessions()
{
    QList<Session> result;
    QSqlQuery query(threadDb());
    query.prepare("SELECT * FROM sessions ORDER BY is_pinned DESC, sort_index ASC");

    if (query.exec()) {
        while (query.next()) {
            Session s = rowToSession(query);

            QSqlQuery msgQuery(threadDb());
            msgQuery.prepare("SELECT * FROM messages WHERE session_id = ? ORDER BY timestamp ASC");
            msgQuery.addBindValue(s.id);
            if (msgQuery.exec()) {
                while (msgQuery.next()) {
                    Message msg = rowToMessage(msgQuery);

                    QSqlQuery mfQuery(threadDb());
                    mfQuery.prepare("SELECT * FROM media_files WHERE message_id = ?");
                    mfQuery.addBindValue(msg.id);
                    if (mfQuery.exec()) {
                        while (mfQuery.next()) {
                            msg.mediaFiles.append(rowToMediaFile(mfQuery));
                        }
                    }

                    s.messages.append(msg);
                }
            }

            QSqlQuery pfQuery(threadDb());
            pfQuery.prepare("SELECT * FROM pending_files WHERE session_id = ?");
            pfQuery.addBindValue(s.id);
            if (pfQuery.exec()) {
                while (pfQuery.next()) {
                    MediaFile pf = rowToMediaFile(pfQuery);
                    s.pendingFiles.append(pf);
                }
            }

            result.append(s);
        }
    }

    return result;
}

QList<Message> DatabaseService::loadSessionMessages(const QString& sessionId)
{
    QList<Message> result;
    QSqlQuery query(threadDb());
    query.prepare("SELECT * FROM messages WHERE session_id = ? ORDER BY timestamp ASC");
    query.addBindValue(sessionId);

    if (query.exec()) {
        while (query.next()) {
            Message msg = rowToMessage(query);

            QSqlQuery mfQuery(threadDb());
            mfQuery.prepare("SELECT * FROM media_files WHERE message_id = ?");
            mfQuery.addBindValue(msg.id);
            if (mfQuery.exec()) {
                while (mfQuery.next()) {
                    msg.mediaFiles.append(rowToMediaFile(mfQuery));
                }
            }

            result.append(msg);
        }
    }

    return result;
}

void DatabaseService::updateSessionCounter(int counter)
{
    setMetadataInt("session_counter", counter);
}

// ===== Messages CRUD =====

bool DatabaseService::insertMessage(const QString& sessionId, const Message& message)
{
    QSqlQuery query(threadDb());
    query.prepare(R"(
        INSERT OR REPLACE INTO messages (
            id, session_id, timestamp, mode, status, progress, queue_position,
            error_message, parameters, shader_params,
            ai_model_id, ai_category, ai_use_gpu, ai_tile_size, ai_auto_tile_size, ai_model_params
        ) VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)
    )");
    query.addBindValue(message.id);
    query.addBindValue(sessionId);
    query.addBindValue(static_cast<qint64>(message.timestamp.toSecsSinceEpoch()));
    query.addBindValue(static_cast<int>(message.mode));
    query.addBindValue(static_cast<int>(message.status));
    query.addBindValue(message.progress);
    query.addBindValue(message.queuePosition);
    query.addBindValue(message.errorMessage);
    query.addBindValue(QString(QJsonDocument(QJsonObject::fromVariantMap(message.parameters)).toJson(QJsonDocument::Compact)));
    query.addBindValue(QString(QJsonDocument(QJsonObject::fromVariantMap(shaderParamsToMap(message.shaderParams))).toJson(QJsonDocument::Compact)));
    query.addBindValue(message.aiParams.modelId);
    query.addBindValue(static_cast<int>(message.aiParams.category));
    query.addBindValue(message.aiParams.useGpu ? 1 : 0);
    query.addBindValue(message.aiParams.tileSize);
    query.addBindValue(message.aiParams.autoTileSize ? 1 : 0);
    query.addBindValue(QString(QJsonDocument(QJsonObject::fromVariantMap(message.aiParams.modelParams)).toJson(QJsonDocument::Compact)));

    if (!query.exec()) {
        qWarning() << "[DB] insertMessage failed for:" << message.id << query.lastError().text()
                   << "nativeError:" << query.lastError().nativeErrorCode()
                   << "driverText:" << query.lastError().driverText()
                   << "databaseText:" << query.lastError().databaseText();
        return false;
    }

    for (const MediaFile& mf : message.mediaFiles) {
        insertMediaFile(message.id, mf);
    }

    return true;
}

bool DatabaseService::updateMessage(const Message& message)
{
    QSqlQuery query(threadDb());
    query.prepare(R"(
        UPDATE messages SET
            timestamp = ?, mode = ?, status = ?, progress = ?, queue_position = ?,
            error_message = ?, parameters = ?, shader_params = ?,
            ai_model_id = ?, ai_category = ?, ai_use_gpu = ?, ai_tile_size = ?,
            ai_auto_tile_size = ?, ai_model_params = ?
        WHERE id = ?
    )");
    query.addBindValue(static_cast<qint64>(message.timestamp.toSecsSinceEpoch()));
    query.addBindValue(static_cast<int>(message.mode));
    query.addBindValue(static_cast<int>(message.status));
    query.addBindValue(message.progress);
    query.addBindValue(message.queuePosition);
    query.addBindValue(message.errorMessage);
    query.addBindValue(QString(QJsonDocument(QJsonObject::fromVariantMap(message.parameters)).toJson(QJsonDocument::Compact)));
    query.addBindValue(QString(QJsonDocument(QJsonObject::fromVariantMap(shaderParamsToMap(message.shaderParams))).toJson(QJsonDocument::Compact)));
    query.addBindValue(message.aiParams.modelId);
    query.addBindValue(static_cast<int>(message.aiParams.category));
    query.addBindValue(message.aiParams.useGpu ? 1 : 0);
    query.addBindValue(message.aiParams.tileSize);
    query.addBindValue(message.aiParams.autoTileSize ? 1 : 0);
    query.addBindValue(QString(QJsonDocument(QJsonObject::fromVariantMap(message.aiParams.modelParams)).toJson(QJsonDocument::Compact)));
    query.addBindValue(message.id);

    if (!query.exec()) {
        qWarning() << "[DB] updateMessage failed:" << query.lastError().text();
        return false;
    }

    for (const MediaFile& mf : message.mediaFiles) {
        updateMediaFile(mf);
    }

    return true;
}

bool DatabaseService::deleteMessage(const QString& messageId)
{
    QSqlQuery query(threadDb());
    query.prepare("DELETE FROM messages WHERE id = ?");
    query.addBindValue(messageId);
    if (!query.exec()) {
        qWarning() << "[DB] deleteMessage failed:" << query.lastError().text();
        return false;
    }
    return true;
}

bool DatabaseService::updateMessageProgress(const QString& messageId, int progress)
{
    QSqlQuery query(threadDb());
    query.prepare("UPDATE messages SET progress = ? WHERE id = ?");
    query.addBindValue(progress);
    query.addBindValue(messageId);
    return query.exec();
}

bool DatabaseService::updateMessageStatus(const QString& messageId, int status)
{
    QSqlQuery query(threadDb());
    query.prepare("UPDATE messages SET status = ? WHERE id = ?");
    query.addBindValue(status);
    query.addBindValue(messageId);
    return query.exec();
}

// ===== MediaFiles CRUD =====

QString DatabaseService::insertMediaFile(const QString& messageId, const MediaFile& mediaFile)
{
    QSqlQuery query(threadDb());
    query.prepare(R"(
        INSERT OR REPLACE INTO media_files (
            id, message_id, pending_file_id, file_path, original_path,
            file_name, file_size, media_type, duration, resolution_w, resolution_h,
            status, result_path
        ) VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)
    )");
    query.addBindValue(mediaFile.id);
    query.addBindValue(messageId);
    query.addBindValue(QString());  // pending_file_id
    query.addBindValue(mediaFile.filePath);
    query.addBindValue(mediaFile.originalPath);
    query.addBindValue(mediaFile.fileName);
    query.addBindValue(mediaFile.fileSize);
    query.addBindValue(static_cast<int>(mediaFile.type));
    query.addBindValue(mediaFile.duration);
    query.addBindValue(mediaFile.resolution.width());
    query.addBindValue(mediaFile.resolution.height());
    query.addBindValue(static_cast<int>(mediaFile.status));
    query.addBindValue(mediaFile.resultPath);

    if (!query.exec()) {
        qWarning() << "[DB] insertMediaFile failed:" << query.lastError().text();
        return QString();
    }
    return mediaFile.id;
}

bool DatabaseService::updateMediaFile(const MediaFile& mediaFile)
{
    QSqlQuery query(threadDb());
    query.prepare(R"(
        UPDATE media_files SET
            file_path = ?, original_path = ?, file_name = ?, file_size = ?,
            media_type = ?, duration = ?, resolution_w = ?, resolution_h = ?,
            status = ?, result_path = ?
        WHERE id = ?
    )");
    query.addBindValue(mediaFile.filePath);
    query.addBindValue(mediaFile.originalPath);
    query.addBindValue(mediaFile.fileName);
    query.addBindValue(mediaFile.fileSize);
    query.addBindValue(static_cast<int>(mediaFile.type));
    query.addBindValue(mediaFile.duration);
    query.addBindValue(mediaFile.resolution.width());
    query.addBindValue(mediaFile.resolution.height());
    query.addBindValue(static_cast<int>(mediaFile.status));
    query.addBindValue(mediaFile.resultPath);
    query.addBindValue(mediaFile.id);

    return query.exec();
}

bool DatabaseService::deleteMediaFile(const QString& fileId)
{
    QSqlQuery query(threadDb());
    query.prepare("DELETE FROM media_files WHERE id = ?");
    query.addBindValue(fileId);
    return query.exec();
}

QList<MediaFile> DatabaseService::loadMessageMediaFiles(const QString& messageId)
{
    QList<MediaFile> result;
    QSqlQuery query(threadDb());
    query.prepare("SELECT * FROM media_files WHERE message_id = ?");
    query.addBindValue(messageId);

    if (query.exec()) {
        while (query.next()) {
            result.append(rowToMediaFile(query));
        }
    }
    return result;
}

int DatabaseService::getMessageMediaFileCount(const QString& messageId) const
{
    QSqlQuery query(threadDb());
    query.prepare("SELECT COUNT(*) FROM media_files WHERE message_id = ?");
    query.addBindValue(messageId);
    if (query.exec() && query.next()) {
        return query.value(0).toInt();
    }
    return 0;
}

// ===== PendingFiles CRUD =====

bool DatabaseService::insertPendingFile(const QString& sessionId, const MediaFile& pendingFile)
{
    QSqlQuery query(threadDb());
    query.prepare(R"(
        INSERT OR REPLACE INTO pending_files (
            id, session_id, file_path, file_name, file_size,
            media_type, duration, width, height, status, result_path
        ) VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)
    )");
    query.addBindValue(pendingFile.id);
    query.addBindValue(sessionId);
    query.addBindValue(pendingFile.filePath);
    query.addBindValue(pendingFile.fileName);
    query.addBindValue(pendingFile.fileSize);
    query.addBindValue(static_cast<int>(pendingFile.type));
    query.addBindValue(pendingFile.duration);
    query.addBindValue(pendingFile.resolution.width());
    query.addBindValue(pendingFile.resolution.height());
    query.addBindValue(static_cast<int>(pendingFile.status));
    query.addBindValue(pendingFile.resultPath);

    if (!query.exec()) {
        qWarning() << "[DB] insertPendingFile failed:" << query.lastError().text();
        return false;
    }
    return true;
}

QList<MediaFile> DatabaseService::loadPendingFiles(const QString& sessionId)
{
    QList<MediaFile> result;
    QSqlQuery query(threadDb());
    query.prepare("SELECT * FROM pending_files WHERE session_id = ?");
    query.addBindValue(sessionId);

    if (query.exec()) {
        while (query.next()) {
            MediaFile f = rowToMediaFile(query);
            result.append(f);
        }
    }
    return result;
}

bool DatabaseService::clearPendingFiles(const QString& sessionId)
{
    QSqlQuery query(threadDb());
    query.prepare("DELETE FROM pending_files WHERE session_id = ?");
    query.addBindValue(sessionId);
    return query.exec();
}

bool DatabaseService::deletePendingFile(const QString& pendingFileId)
{
    QSqlQuery query(threadDb());
    query.prepare("DELETE FROM pending_files WHERE id = ?");
    query.addBindValue(pendingFileId);
    return query.exec();
}

// ===== Thumbnails CRUD =====

bool DatabaseService::saveThumbnail(const QString& mediaFileId, const QImage& thumbnail)
{
    if (thumbnail.isNull() || mediaFileId.isEmpty()) return false;

    QByteArray ba;
    QBuffer buffer(&ba);
    buffer.open(QIODevice::WriteOnly);
    thumbnail.save(&buffer, "PNG");
    buffer.close();

    QSqlQuery query(threadDb());
    query.prepare(R"(
        INSERT OR REPLACE INTO thumbnails (media_file_id, thumbnail, width, height, updated_at)
        VALUES (?, ?, ?, ?, strftime('%s','now'))
    )");
    query.addBindValue(mediaFileId);
    query.addBindValue(ba);
    query.addBindValue(thumbnail.width());
    query.addBindValue(thumbnail.height());

    if (!query.exec()) {
        qWarning() << "[DB] saveThumbnail failed for:" << mediaFileId << query.lastError().text();
        return false;
    }
    return true;
}

QImage DatabaseService::loadThumbnail(const QString& mediaFileId) const
{
    QSqlQuery query(threadDb());
    query.prepare("SELECT thumbnail FROM thumbnails WHERE media_file_id = ?");
    query.addBindValue(mediaFileId);

    if (query.exec() && query.next()) {
        QByteArray ba = query.value(0).toByteArray();
        QImage img;
        img.loadFromData(ba, "PNG");
        return img;
    }
    return QImage();
}

bool DatabaseService::deleteThumbnail(const QString& mediaFileId)
{
    QSqlQuery query(threadDb());
    query.prepare("DELETE FROM thumbnails WHERE media_file_id = ?");
    query.addBindValue(mediaFileId);
    return query.exec();
}

bool DatabaseService::thumbnailExists(const QString& mediaFileId) const
{
    QSqlQuery query(threadDb());
    query.prepare("SELECT COUNT(*) FROM thumbnails WHERE media_file_id = ?");
    query.addBindValue(mediaFileId);
    if (query.exec() && query.next()) {
        return query.value(0).toInt() > 0;
    }
    return false;
}

int DatabaseService::thumbnailCount() const
{
    QSqlQuery query(threadDb());
    if (query.exec("SELECT COUNT(*) FROM thumbnails") && query.next()) {
        return query.value(0).toInt();
    }
    return 0;
}

qint64 DatabaseService::totalThumbnailSize() const
{
    QSqlQuery query(threadDb());
    if (query.exec("SELECT COALESCE(SUM(length(thumbnail)), 0) FROM thumbnails") && query.next()) {
        return query.value(0).toLongLong();
    }
    return 0;
}

// ===== 批量操作 =====

bool DatabaseService::saveFullSession(const Session& session, const QList<Message>& messages)
{
    if (session.id.isEmpty()) {
        qWarning() << "[DB] saveFullSession: empty session ID";
        return false;
    }

    if (!beginTransaction()) {
        qWarning() << "[DB] saveFullSession: failed to begin transaction for session" << session.id;
        return false;
    }

    bool ok = true;

    if (!insertSession(session)) {
        qWarning() << "[DB] saveFullSession: insertSession FAILED for" << session.id;
        ok = false;
        goto cleanup;
    }

    for (int i = 0; i < messages.size(); ++i) {
        const Message& msg = messages.at(i);
        if (!insertMessage(session.id, msg)) {
            qWarning() << "[DB] saveFullSession: insertMessage FAILED for message" << msg.id;
            ok = false;
            goto cleanup;
        }
    }

cleanup:
    if (ok) {
        ok = commitTransaction();
    } else {
        rollbackTransaction();
    }
    return ok;
}

// ===== 会话级清理（级联）=====

bool DatabaseService::clearSessionData(const QString& sessionId)
{
    if (!beginTransaction()) return false;

    QSqlQuery query(threadDb());
    query.prepare("DELETE FROM messages WHERE session_id = ?");
    query.addBindValue(sessionId);
    if (!query.exec()) { rollbackTransaction(); return false; }

    query.prepare("DELETE FROM pending_files WHERE session_id = ?");
    query.addBindValue(sessionId);
    if (!query.exec()) { rollbackTransaction(); return false; }

    return commitTransaction();
}

bool DatabaseService::deleteSessions(const QStringList& sessionIds)
{
    if (!beginTransaction()) return false;

    for (const QString& id : sessionIds) {
        if (!deleteSession(id)) { rollbackTransaction(); return false; }
    }

    return commitTransaction();
}

// ===== 按条件清理 =====

int DatabaseService::clearAllMessagesByMode(int mode)
{
    QSqlQuery query(threadDb());
    query.prepare("DELETE FROM messages WHERE mode = ?");
    query.addBindValue(mode);
    if (query.exec()) {
        return query.numRowsAffected();
    }
    return 0;
}

int DatabaseService::clearAllShaderVideoMessages()
{
    QSqlQuery query(threadDb());
    query.prepare(R"(
        DELETE FROM messages WHERE id IN (
            SELECT DISTINCT mf.message_id FROM media_files mf
            INNER JOIN messages m ON mf.message_id = m.id
            WHERE m.mode = 0 AND mf.media_type = 1
        )
    )");
    if (query.exec()) {
        return query.numRowsAffected();
    }
    return 0;
}

int DatabaseService::clearMediaFilesByModeAndType(int mode, int mediaType)
{
    if (!beginTransaction()) return -1;

    int totalDeleted = 0;

    QSqlQuery findQuery(threadDb());
    findQuery.prepare(R"(
        SELECT DISTINCT message_id FROM media_files mf
        INNER JOIN messages m ON mf.message_id = m.id
        WHERE m.mode = ? AND mf.media_type = ?
    )");
    findQuery.addBindValue(mode);
    findQuery.addBindValue(mediaType);

    if (findQuery.exec()) {
        QStringList affectedMsgIds;
        while (findQuery.next()) {
            affectedMsgIds.append(findQuery.value(0).toString());
        }

        for (const QString& msgId : affectedMsgIds) {
            QSqlQuery delMfQuery(threadDb());
            delMfQuery.prepare("DELETE FROM media_files WHERE message_id = ? AND media_type = ?");
            delMfQuery.addBindValue(msgId);
            delMfQuery.addBindValue(mediaType);
            if (delMfQuery.exec()) {
                totalDeleted += delMfQuery.numRowsAffected();
            }

            if (getMessageMediaFileCount(msgId) == 0) {
                deleteMessage(msgId);
            }
        }
    }

    commitTransaction();
    return totalDeleted;
}

int DatabaseService::clearAllMessages()
{
    QSqlQuery query(threadDb());
    if (query.exec("DELETE FROM messages")) {
        return query.numRowsAffected();
    }
    return 0;
}

bool DatabaseService::deleteAllData()
{
    if (!beginTransaction()) return false;

    execSql("DELETE FROM thumbnails");
    execSql("DELETE FROM media_files");
    execSql("DELETE FROM pending_files");
    execSql("DELETE FROM messages");
    execSql("DELETE FROM sessions");

    return commitTransaction();
}

// ===== 缩略图专项清理 =====

int DatabaseService::clearThumbnailsByPathPrefix(const QString& pathPrefix)
{
    QSqlQuery query(threadDb());
    query.prepare(R"(
        DELETE FROM thumbnails WHERE media_file_id IN (
            SELECT mf.id FROM media_files mf
            WHERE mf.file_path LIKE ? OR mf.result_path LIKE ?
        )
    )");
    QString likePattern = pathPrefix + "%";
    query.addBindValue(likePattern);
    query.addBindValue(likePattern);

    if (query.exec()) {
        return query.numRowsAffected();
    }
    return 0;
}

int DatabaseService::clearAllThumbnails()
{
    QSqlQuery query(threadDb());
    if (query.exec("DELETE FROM thumbnails")) {
        return query.numRowsAffected();
    }
    return 0;
}

int DatabaseService::cleanupOrphanedThumbnails()
{
    QSqlQuery query(threadDb());
    if (query.exec(
        "DELETE FROM thumbnails WHERE media_file_id NOT IN (SELECT id FROM media_files)"
    )) {
        return query.numRowsAffected();
    }
    return 0;
}

// ===== 统计查询 =====

QVariantMap DatabaseService::getTableStats() const
{
    QVariantMap stats;
    stats["sessions"] = 0;
    stats["messages"] = 0;
    stats["media_files"] = 0;
    stats["pending_files"] = 0;
    stats["thumbnails"] = 0;

    static const char* tables[] = {"sessions", "messages", "media_files", "pending_files", "thumbnails"};
    for (const char* tbl : tables) {
        QSqlQuery q(threadDb());
        q.exec(QString("SELECT COUNT(*) FROM %1").arg(tbl));
        if (q.next()) {
            stats[tbl] = q.value(0).toInt();
        }
    }

    return stats;
}

qint64 DatabaseService::databaseFileSize() const
{
    QFileInfo fi(m_dbPath);
    return fi.exists() ? fi.size() : 0;
}

// ===== 迁移相关 =====

void DatabaseService::checkAndMigrate()
{
    int dbVersion = getMetadataInt("version", 1);

    if (dbVersion < CURRENT_SCHEMA_VERSION) {
        qInfo() << "[DB] Migrating schema from v" << dbVersion << "to v" << CURRENT_SCHEMA_VERSION;

        createBackupBeforeMigration(dbVersion);

        for (int v = dbVersion; v < CURRENT_SCHEMA_VERSION; ++v) {
            bool ok = runMigrationStep(v, v + 1);
            if (!ok) {
                qCritical() << "[DB] Migration failed at step v" << v << "->v" << (v + 1);
                restoreFromBackup();
                break;
            }
        }

        setMetadataInt("version", CURRENT_SCHEMA_VERSION);
    } else if (dbVersion > CURRENT_SCHEMA_VERSION) {
        qWarning() << "[DB] Database version" << dbVersion
                   << "is newer than application version" << CURRENT_SCHEMA_VERSION;
    }
}

bool DatabaseService::runMigrationStep(int fromVersion, int toVersion)
{
    if (fromVersion == 1 && toVersion == 2) {
        return migrateV1toV2("");
    }
    qWarning() << "[DB] Unknown migration step: v" << fromVersion << "->v" << toVersion;
    return false;
}

bool DatabaseService::migrateV1toV2(const QString& jsonFilePath)
{
    QString filePath = jsonFilePath.isEmpty()
        ? QFileInfo(m_dbPath).absolutePath() + "/sessions.json"
        : jsonFilePath;

    QFile file(filePath);
    if (!file.exists()) {
        qInfo() << "[DB] No JSON file found for migration (fresh install or already migrated)";
        return true;
    }

    if (!file.open(QIODevice::ReadOnly)) {
        qCritical() << "[DB] Cannot open JSON for migration:" << filePath;
        return false;
    }

    QJsonParseError parseError;
    QJsonDocument doc = QJsonDocument::fromJson(file.readAll(), &parseError);
    file.close();

    if (parseError.error != QJsonParseError::NoError) {
        qCritical() << "[DB] JSON parse error:" << parseError.errorString();
        return false;
    }

    QJsonObject root = doc.object();
    int sessionCounter = root["session_counter"].toInt(0);
    QString lastActiveId = root["lastActiveSessionId"].toString();

    QJsonArray sessionsArray = root["sessions"].toArray();
    int migratedCount = 0;

    if (!beginTransaction()) return false;

    for (const QJsonValue& sv : sessionsArray) {
        QJsonObject sj = sv.toObject();
        Session session;
        session.id = sj["id"].toString();
        session.name = sj["name"].toString();
        session.createdAt = QDateTime::fromString(sj["createdAt"].toString(), Qt::ISODate);
        session.modifiedAt = QDateTime::fromString(sj["modifiedAt"].toString(), Qt::ISODate);
        session.isActive = sj["isActive"].toBool(false);
        session.isSelected = sj["isSelected"].toBool(false);
        session.isPinned = sj["isPinned"].toBool(false);
        session.sortIndex = sj["sortIndex"].toInt(0);

        if (!insertSession(session)) continue;

        QJsonArray messagesArray = sj["messages"].toArray();
        for (const QJsonValue& mv : messagesArray) {
            QJsonObject mj = mv.toObject();
            Message msg;
            msg.id = mj["id"].toString();
            msg.timestamp = QDateTime::fromString(mj["timestamp"].toString(), Qt::ISODate);
            msg.mode = static_cast<ProcessingMode>(mj["mode"].toInt(0));
            msg.status = static_cast<ProcessingStatus>(mj["status"].toInt(0));
            msg.errorMessage = mj["errorMessage"].toString();
            msg.progress = mj["progress"].toInt(0);
            msg.queuePosition = mj["queuePosition"].toInt(-1);

            QJsonDocument paramsDoc(mj["parameters"].toObject());
            msg.parameters = paramsDoc.object().toVariantMap();

            QJsonDocument shaderDoc(mj["shaderParams"].toObject());
            msg.shaderParams = mapToShaderParams(shaderDoc.object().toVariantMap());

            if (mj.contains("aiParams")) {
                QJsonObject aj = mj["aiParams"].toObject();
                msg.aiParams.modelId = aj["modelId"].toString();
                msg.aiParams.category = static_cast<ModelCategory>(aj["category"].toInt(0));
                msg.aiParams.useGpu = aj["useGpu"].toBool(true);
                msg.aiParams.tileSize = aj["tileSize"].toInt(0);
                msg.aiParams.modelParams = aj["modelParams"].toObject().toVariantMap();
            }

            QJsonArray filesArray = mj["mediaFiles"].toArray();
            for (const QJsonValue& fv : filesArray) {
                QJsonObject fj = fv.toObject();
                MediaFile mf;
                mf.id = fj["id"].toString();
                mf.filePath = fj["filePath"].toString();
                mf.fileName = fj["fileName"].toString();
                mf.fileSize = fj["fileSize"].toVariant().toLongLong();
                mf.type = static_cast<MediaType>(fj["type"].toInt(0));
                mf.duration = fj["duration"].toVariant().toLongLong();
                mf.resolution = QSize(fj["width"].toInt(0), fj["height"].toInt(0));
                mf.status = static_cast<ProcessingStatus>(fj["status"].toInt(0));
                if (mf.status == ProcessingStatus::Processing) {
                    mf.status = ProcessingStatus::Pending;
                }
                mf.resultPath = fj["resultPath"].toString();

                insertMediaFile(msg.id, mf);
            }

            if (!insertMessage(session.id, msg)) continue;
            migratedCount++;
        }

        QJsonArray pendingArray = sj["pendingFiles"].toArray();
        for (const QJsonValue& pv : pendingArray) {
            QJsonObject pj = pv.toObject();
            MediaFile pf;
            pf.id = pj["id"].toString();
            pf.filePath = pj["filePath"].toString();
            pf.fileName = pj["fileName"].toString();
            pf.fileSize = pj["fileSize"].toVariant().toLongLong();
            pf.type = static_cast<MediaType>(pj["type"].toInt(0));
            pf.duration = pj["duration"].toVariant().toLongLong();
            pf.resolution = QSize(pj["width"].toInt(0), pj["height"].toInt(0));
            pf.status = static_cast<ProcessingStatus>(pj["status"].toInt(0));
            pf.resultPath = pj["resultPath"].toString();

            insertPendingFile(session.id, pf);
        }
    }

    setMetadataInt("session_counter", sessionCounter);
    setMetadata("last_active_session_id", lastActiveId);

    if (!commitTransaction()) return false;

    qInfo() << "[DB] V1->V2 migration completed:" << migratedCount << "sessions migrated";

    QString backupPath = m_dbPath + ".json.migrated";
    QFile::rename(filePath, backupPath);
    qInfo() << "[DB] Original JSON backed up to:" << backupPath;

    return true;
}

bool DatabaseService::createBackupBeforeMigration(int currentVersion)
{
    QString backupPath = m_dbPath + ".bak_v" + QString::number(currentVersion);
    if (QFile::exists(backupPath)) {
        QFile::remove(backupPath);
    }
    bool ok = QFile::copy(m_dbPath, backupPath);
    qInfo() << "[DB] Pre-migration backup created:" << backupPath << "success:" << ok;
    return ok;
}

bool DatabaseService::restoreFromBackup()
{
    QDir dir = QFileInfo(m_dbPath).absoluteDir();
    QStringList backups = dir.entryList({"*.db.bak_*"}, QDir::Files | QDir::NoDotAndDotDot, QDir::Name | QDir::Reversed);

    if (backups.isEmpty()) {
        qCritical() << "[DB] No backup found to restore from";
        return false;
    }

    QString latestBackup = backups.first();
    qInfo() << "[DB] Restoring from backup:" << latestBackup;

    if (m_db.isOpen()) {
        m_db.close();
    }

    QFile::remove(m_dbPath + "-wal");
    QFile::remove(m_dbPath + "-shm");
    bool ok = QFile::copy(dir.filePath(latestBackup), m_dbPath);

    if (ok) {
        m_db.open();
        createTables();
        checkAndMigrate();
    }

    return ok;
}

} // namespace EnhanceVision
