#include "EnhanceVision/core/ThumbnailDatabase.h"
#include <QDir>
#include <QFileInfo>
#include <QSqlQuery>
#include <QSqlRecord>
#include <QSqlError>
#include <QDebug>
#include <QDateTime>
#include <QCryptographicHash>
#include <QStandardPaths>

namespace EnhanceVision {

ThumbnailDatabase* ThumbnailDatabase::s_instance = nullptr;

ThumbnailDatabase::ThumbnailDatabase(QObject* parent)
    : QObject(parent)
{
}

ThumbnailDatabase::~ThumbnailDatabase()
{
    close();
    if (s_instance == this) {
        s_instance = nullptr;
    }
}

ThumbnailDatabase* ThumbnailDatabase::instance()
{
    if (!s_instance) {
        s_instance = new ThumbnailDatabase();
    }
    return s_instance;
}

void ThumbnailDatabase::destroyInstance()
{
    if (s_instance) {
        delete s_instance;
        s_instance = nullptr;
    }
}

bool ThumbnailDatabase::initialize(const QString& dataDirPath)
{
    QMutexLocker locker(&m_mutex);

    if (m_initialized) {
        return true;
    }

    QDir dataDir(dataDirPath);
    if (!dataDir.exists()) {
        dataDir.mkpath(".");
    }

    m_thumbnailDir = dataDir.filePath("thumbnails");
    if (!ensureThumbnailDir()) {
        qWarning() << "[ThumbnailDatabase] Failed to create thumbnail directory:" << m_thumbnailDir;
    }

    m_dbPath = dataDir.filePath("thumbnail_meta.db");

    m_db = QSqlDatabase::addDatabase("QSQLITE", "thumbnail_db_conn");
    m_db.setDatabaseName(m_dbPath);

    if (!m_db.open()) {
        qCritical() << "[ThumbnailDatabase] Failed to open database:" << m_db.lastError().text();
        emit databaseInitialized(false);
        return false;
    }

    m_db.exec("PRAGMA journal_mode=WAL");
    m_db.exec("PRAGMA synchronous=NORMAL");
    m_db.exec("PRAGMA cache_size=-2000");

    if (!createTable()) {
        qCritical() << "[ThumbnailDatabase] Failed to create tables";
        m_db.close();
        emit databaseInitialized(false);
        return false;
    }

    int count = 0;
    qint64 diskSize = 0;

    {
        QSqlQuery countQuery(m_db);
        if (countQuery.exec("SELECT COUNT(*) FROM thumbnails") && countQuery.next()) {
            count = countQuery.value(0).toInt();
        }
    }

    {
        QSqlQuery sizeQuery(m_db);
        if (sizeQuery.exec("SELECT SUM(file_size) FROM thumbnails WHERE status = 0 AND file_size > 0") && sizeQuery.next()) {
            diskSize = sizeQuery.value(0).toLongLong();
        }
    }

    m_initialized = true;

    qInfo() << "[ThumbnailDatabase] Initialized successfully at:" << m_dbPath
            << "- entries:" << count << "- disk size:" << diskSize / 1024 << "KB";

    emit databaseInitialized(true);
    return true;
}

void ThumbnailDatabase::close()
{
    QMutexLocker locker(&m_mutex);

    if (m_db.isOpen()) {
        m_db.close();
    }
    QSqlDatabase::removeDatabase("thumbnail_db_conn");
    m_initialized = false;
}

bool ThumbnailDatabase::createTable()
{
    QSqlQuery query(m_db);

    QString sql = R"(
        CREATE TABLE IF NOT EXISTS thumbnails (
            cache_key     TEXT PRIMARY KEY,
            file_path     TEXT NOT NULL,
            disk_path     TEXT DEFAULT '',
            file_hash     TEXT DEFAULT '',
            width         INTEGER NOT NULL DEFAULT 0,
            height        INTEGER NOT NULL DEFAULT 0,
            file_size     INTEGER NOT NULL DEFAULT 0,
            generated_at  INTEGER NOT NULL DEFAULT 0,
            last_accessed INTEGER NOT NULL DEFAULT 0,
            access_count  INTEGER NOT NULL DEFAULT 0,
            status        INTEGER NOT NULL DEFAULT 0,
            fail_count    INTEGER NOT NULL DEFAULT 0,
            fail_reason   TEXT DEFAULT '',
            last_error    TEXT DEFAULT ''
        )
    )";

    if (!query.exec(sql)) {
        qCritical() << "[ThumbnailDatabase] CREATE TABLE failed:" << query.lastError().text();
        return false;
    }

    QStringList indexes = {
        "CREATE INDEX IF NOT EXISTS idx_thumbnails_status ON thumbnails(status)",
        "CREATE INDEX IF NOT EXISTS idx_thumbnails_file_path ON thumbnails(file_path)",
        "CREATE INDEX IF NOT EXISTS idx_thumbnails_generated_at ON thumbnails(generated_at)",
        "CREATE INDEX IF NOT EXISTS idx_thumbnails_last_accessed ON thumbnails(last_accessed)"
    };

    for (const QString& idxSql : indexes) {
        if (!query.exec(idxSql)) {
            qWarning() << "[ThumbnailDatabase] Create index warning:" << query.lastError().text();
        }
    }

    return true;
}

bool ThumbnailDatabase::ensureThumbnailDir()
{
    QDir dir(m_thumbnailDir);
    if (!dir.exists()) {
        return dir.mkpath(".");
    }
    return true;
}

bool ThumbnailDatabase::upsertMetadata(const ThumbnailMeta& meta)
{
    QMutexLocker locker(&m_mutex);

    if (!m_initialized || !m_db.isOpen()) return false;

    QSqlQuery query(m_db);
    query.prepare(R"(
        INSERT OR REPLACE INTO thumbnails (
            cache_key, file_path, disk_path, file_hash, width, height,
            file_size, generated_at, last_accessed, access_count, status,
            fail_count, fail_reason, last_error
        ) VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)
    )");

    query.addBindValue(meta.cacheKey);
    query.addBindValue(meta.filePath);
    query.addBindValue(meta.diskPath);
    query.addBindValue(meta.fileHash);
    query.addBindValue(meta.width);
    query.addBindValue(meta.height);
    query.addBindValue(meta.fileSize);
    query.addBindValue(meta.generatedAt > 0 ? meta.generatedAt : QDateTime::currentSecsSinceEpoch());
    query.addBindValue(meta.lastAccessed > 0 ? meta.lastAccessed : QDateTime::currentSecsSinceEpoch());
    query.addBindValue(meta.accessCount > 0 ? meta.accessCount : 1);
    query.addBindValue(meta.status);
    query.addBindValue(meta.failCount);
    query.addBindValue(meta.failReason);
    query.addBindValue(meta.lastError);

    if (!query.exec()) {
        qWarning() << "[ThumbnailDatabase] upsertMetadata failed for" << meta.cacheKey
                   << ":" << query.lastError().text();
        return false;
    }

    emit metadataChanged();
    return true;
}

ThumbnailMeta ThumbnailDatabase::getMetadata(const QString& cacheKey)
{
    QMutexLocker locker(&m_mutex);

    ThumbnailMeta meta;
    if (!m_initialized || !m_db.isOpen()) return meta;

    QSqlQuery query(m_db);
    query.prepare("SELECT * FROM thumbnails WHERE cache_key = ?");
    query.addBindValue(cacheKey);

    if (!query.exec() || !query.next()) {
        return meta;
    }

    const QSqlRecord rec = query.record();

    meta.cacheKey = rec.value("cache_key").toString();
    meta.filePath = rec.value("file_path").toString();
    meta.diskPath = rec.value("disk_path").toString();
    meta.fileHash = rec.value("file_hash").toString();
    meta.width = rec.value("width").toInt();
    meta.height = rec.value("height").toInt();
    meta.fileSize = rec.value("file_size").toLongLong();
    meta.generatedAt = rec.value("generated_at").toLongLong();
    meta.lastAccessed = rec.value("last_accessed").toLongLong();
    meta.accessCount = rec.value("access_count").toInt();
    meta.status = rec.value("status").toInt();
    meta.failCount = rec.value("fail_count").toInt();
    meta.failReason = rec.value("fail_reason").toString();
    meta.lastError = rec.value("last_error").toString();

    return meta;
}

QList<ThumbnailMeta> ThumbnailDatabase::getAllValid(int limit, const QString& orderBy)
{
    QMutexLocker locker(&m_mutex);

    QList<ThumbnailMeta> results;
    if (!m_initialized || !m_db.isOpen()) return results;

    QString sql = "SELECT * FROM thumbnails WHERE status = 0 ORDER BY " + orderBy;
    if (limit > 0) {
        sql += " LIMIT " + QString::number(limit);
    }

    QSqlQuery query(m_db);
    if (!query.exec(sql)) {
        qWarning() << "[ThumbnailDatabase] getAllValid failed:" << query.lastError().text();
        return results;
    }

    while (query.next()) {
        const QSqlRecord rec = query.record();
        ThumbnailMeta meta;
        meta.cacheKey = rec.value("cache_key").toString();
        meta.filePath = rec.value("file_path").toString();
        meta.diskPath = rec.value("disk_path").toString();
        meta.fileHash = rec.value("file_hash").toString();
        meta.width = rec.value("width").toInt();
        meta.height = rec.value("height").toInt();
        meta.fileSize = rec.value("file_size").toLongLong();
        meta.generatedAt = rec.value("generated_at").toLongLong();
        meta.lastAccessed = rec.value("last_accessed").toLongLong();
        meta.accessCount = rec.value("access_count").toInt();
        meta.status = rec.value("status").toInt();
        meta.failCount = rec.value("fail_count").toInt();
        results.append(meta);
    }

    return results;
}

QList<ThumbnailMeta> ThumbnailDatabase::getByStatus(int status)
{
    QMutexLocker locker(&m_mutex);

    QList<ThumbnailMeta> results;
    if (!m_initialized || !m_db.isOpen()) return results;

    QSqlQuery query(m_db);
    query.prepare("SELECT * FROM thumbnails WHERE status = ? ORDER BY generated_at DESC");
    query.addBindValue(status);

    if (!query.exec()) {
        qWarning() << "[ThumbnailDatabase] getByStatus failed:" << query.lastError().text();
        return results;
    }

    while (query.next()) {
        const QSqlRecord rec = query.record();
        ThumbnailMeta meta;
        meta.cacheKey = rec.value("cache_key").toString();
        meta.filePath = rec.value("file_path").toString();
        meta.diskPath = rec.value("disk_path").toString();
        meta.status = rec.value("status").toInt();
        meta.failCount = rec.value("fail_count").toInt();
        results.append(meta);
    }

    return results;
}

bool ThumbnailDatabase::updateStatus(const QString& cacheKey, int status)
{
    QMutexLocker locker(&m_mutex);

    if (!m_initialized || !m_db.isOpen()) return false;

    QSqlQuery query(m_db);
    query.prepare("UPDATE thumbnails SET status = ? WHERE cache_key = ?");
    query.addBindValue(status);
    query.addBindValue(cacheKey);

    bool ok = query.exec();
    if (ok) emit metadataChanged();
    else qWarning() << "[ThumbnailDatabase] updateStatus failed:" << query.lastError().text();

    return ok;
}

bool ThumbnailDatabase::updateAccessTime(const QString& cacheKey)
{
    QMutexLocker locker(&m_mutex);

    if (!m_initialized || !m_db.isOpen()) return false;

    QSqlQuery query(m_db);
    query.prepare("UPDATE thumbnails SET last_accessed = ?, access_count = access_count + 1 WHERE cache_key = ?");
    query.addBindValue(QDateTime::currentSecsSinceEpoch());
    query.addBindValue(cacheKey);

    return query.exec();
}

bool ThumbnailDatabase::incrementFailCount(const QString& cacheKey, const QString& error)
{
    QMutexLocker locker(&m_mutex);

    if (!m_initialized || !m_db.isOpen()) return false;

    QSqlQuery query(m_db);
    query.prepare("UPDATE thumbnails SET status = 1, fail_count = fail_count + 1, last_error = ?, "
                   "last_accessed = ? WHERE cache_key = ?");
    query.addBindValue(error.left(256));
    query.addBindValue(QDateTime::currentSecsSinceEpoch());
    query.addBindValue(cacheKey);

    bool ok = query.exec();
    if (ok) emit metadataChanged();
    return ok;
}

bool ThumbnailDatabase::removeMetadata(const QString& cacheKey)
{
    QMutexLocker locker(&m_mutex);

    if (!m_initialized || !m_db.isOpen()) return false;

    QSqlQuery query(m_db);
    query.prepare("DELETE FROM thumbnails WHERE cache_key = ?");
    query.addBindValue(cacheKey);

    bool ok = query.exec();
    if (ok) emit metadataChanged();
    return ok;
}

int ThumbnailDatabase::clearAll()
{
    QMutexLocker locker(&m_mutex);

    if (!m_initialized || !m_db.isOpen()) return 0;

    int removed = 0;

    QSqlQuery delQuery(m_db);
    if (delQuery.exec("SELECT disk_path FROM thumbnails WHERE disk_path != ''")) {
        while (delQuery.next()) {
            QString dp = delQuery.value(0).toString();
            if (!dp.isEmpty()) QFile::remove(dp);
        }
    }

    QSqlQuery query(m_db);
    if (query.exec("DELETE FROM thumbnails")) {
        removed = query.numRowsAffected();
    }

    emit metadataChanged();
    qInfo() << "[ThumbnailDatabase] clearAll: removed" << removed << "entries";
    return removed;
}

int ThumbnailDatabase::clearByPathPrefix(const QString& prefix)
{
    QMutexLocker locker(&m_mutex);

    if (!m_initialized || !m_db.isOpen() || prefix.isEmpty()) return 0;

    int removed = 0;

    QSqlQuery fetchQuery(m_db);
    fetchQuery.prepare("SELECT disk_path, cache_key FROM thumbnails WHERE file_path LIKE ? OR disk_path LIKE ?");
    fetchQuery.addBindValue(prefix + "%");
    fetchQuery.addBindValue(prefix + "%");

    if (fetchQuery.exec()) {
        while (fetchQuery.next()) {
            QString dp = fetchQuery.value(0).toString();
            if (!dp.isEmpty()) QFile::remove(dp);
        }
    }

    QSqlQuery delQuery(m_db);
    delQuery.prepare("DELETE FROM thumbnails WHERE file_path LIKE ? OR disk_path LIKE ?");
    delQuery.addBindValue(prefix + "%");
    delQuery.addBindValue(prefix + "%");

    if (delQuery.exec()) {
        removed = delQuery.numRowsAffected();
    }

    emit metadataChanged();
    qInfo() << "[ThumbnailDatabase] clearByPathPrefix: removed" << removed << "entries for prefix:" << prefix;
    return removed;
}

int ThumbnailDatabase::clearStale(qint64 maxAgeSec)
{
    QMutexLocker locker(&m_mutex);

    if (!m_initialized || !m_db.isOpen()) return 0;

    qint64 cutoff = QDateTime::currentSecsSinceEpoch() - maxAgeSec;

    QSqlQuery fetchQuery(m_db);
    fetchQuery.prepare("SELECT disk_path FROM thumbnails WHERE last_accessed < ? AND status = 0");
    fetchQuery.addBindValue(cutoff);

    if (fetchQuery.exec()) {
        while (fetchQuery.next()) {
            QString dp = fetchQuery.value(0).toString();
            if (!dp.isEmpty()) QFile::remove(dp);
        }
    }

    QSqlQuery delQuery(m_db);
    delQuery.prepare("DELETE FROM thumbnails WHERE last_accessed < ?");
    delQuery.addBindValue(cutoff);

    int removed = 0;
    if (delQuery.exec()) {
        removed = delQuery.numRowsAffected();
    }

    emit metadataChanged();
    return removed;
}

int ThumbnailDatabase::clearFailed()
{
    QMutexLocker locker(&m_mutex);

    if (!m_initialized || !m_db.isOpen()) return 0;

    QSqlQuery query(m_db);
    if (!query.exec("DELETE FROM thumbnails WHERE status = 1")) {
        return 0;
    }

    int removed = query.numRowsAffected();
    emit metadataChanged();
    return removed;
}

qint64 ThumbnailDatabase::totalDiskSize() const
{
    QDir dir(m_thumbnailDir);
    if (!dir.exists()) return 0;

    qint64 size = 0;
    const auto entries = dir.entryInfoList(QDir::Files | QDir::NoDotAndDotDot);
    for (const QFileInfo& fi : entries) {
        size += fi.size();
    }
    return size;
}

int ThumbnailDatabase::totalCount() const
{
    QMutexLocker locker(const_cast<QMutex*>(&m_mutex));

    if (!m_initialized || !m_db.isOpen()) return 0;

    QSqlQuery query(m_db);
    if (query.exec("SELECT COUNT(*) FROM thumbnails") && query.next()) {
        return query.value(0).toInt();
    }
    return 0;
}

int ThumbnailDatabase::validCount() const
{
    QMutexLocker locker(const_cast<QMutex*>(&m_mutex));

    if (!m_initialized || !m_db.isOpen()) return 0;

    QSqlQuery query(m_db);
    if (query.exec("SELECT COUNT(*) FROM thumbnails WHERE status = 0") && query.next()) {
        return query.value(0).toInt();
    }
    return 0;
}

int ThumbnailDatabase::failedCount() const
{
    QMutexLocker locker(const_cast<QMutex*>(&m_mutex));

    if (!m_initialized || !m_db.isOpen()) return 0;

    QSqlQuery query(m_db);
    if (query.exec("SELECT COUNT(*) FROM thumbnails WHERE status = 1") && query.next()) {
        return query.value(0).toInt();
    }
    return 0;
}

qint64 ThumbnailDatabase::validDiskSize() const
{
    QMutexLocker locker(const_cast<QMutex*>(&m_mutex));

    if (!m_initialized || !m_db.isOpen()) return 0;

    qint64 size = 0;
    QSqlQuery query(m_db);
    if (query.exec("SELECT SUM(file_size) FROM thumbnails WHERE status = 0 AND file_size > 0") && query.next()) {
        size = query.value(0).toLongLong();
    }
    return size;
}

QString ThumbnailDatabase::diskPathForKey(const QString& cacheKey) const
{
    QByteArray hash = QCryptographicHash::hash(
        cacheKey.toUtf8(), QCryptographicHash::Md5);
    return m_thumbnailDir + "/" + hash.toHex().left(16) + ".png";
}

} // namespace EnhanceVision
