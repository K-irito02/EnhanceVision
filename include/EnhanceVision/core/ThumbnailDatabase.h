#ifndef ENHANCEVISION_THUMBNAILDATABASE_H
#define ENHANCEVISION_THUMBNAILDATABASE_H

#include <QObject>
#include <QString>
#include <QList>
#include <QDateTime>
#include <QSqlDatabase>
#include <QMutex>

namespace EnhanceVision {

enum class ThumbStatus : int {
    Valid = 0,
    Failed = 1,
    Pending = 2,
    Stale = 3
};

struct ThumbnailMeta {
    QString cacheKey;
    QString filePath;
    QString diskPath;
    QString fileHash;
    int width = 0;
    int height = 0;
    qint64 fileSize = 0;
    qint64 generatedAt = 0;
    qint64 lastAccessed = 0;
    int accessCount = 0;
    int status = static_cast<int>(ThumbStatus::Valid);
    int failCount = 0;
    QString failReason;
    QString lastError;

    bool isValid() const { return status == static_cast<int>(ThumbStatus::Valid); }
    bool isFailed() const { return status == static_cast<int>(ThumbStatus::Failed); }
};

class ThumbnailDatabase : public QObject
{
    Q_OBJECT

public:
    static ThumbnailDatabase* instance();
    static void destroyInstance();

    bool initialize(const QString& dataDirPath);
    void close();
    bool isInitialized() const { return m_initialized; }

    bool upsertMetadata(const ThumbnailMeta& meta);
    ThumbnailMeta getMetadata(const QString& cacheKey);
    QList<ThumbnailMeta> getAllValid(int limit = -1, const QString& orderBy = "last_accessed DESC");
    QList<ThumbnailMeta> getByStatus(int status);
    bool updateStatus(const QString& cacheKey, int status);
    bool updateAccessTime(const QString& cacheKey);
    bool incrementFailCount(const QString& cacheKey, const QString& error);
    bool removeMetadata(const QString& cacheKey);

    int clearAll();
    int clearByPathPrefix(const QString& prefix);
    int clearStale(qint64 maxAgeSec);
    int clearFailed();

    qint64 totalDiskSize() const;
    int totalCount() const;
    int validCount() const;
    int failedCount() const;
    qint64 validDiskSize() const;

    QString thumbnailDirPath() const { return m_thumbnailDir; }
    QString dbFilePath() const { return m_dbPath; }
    QString diskPathForKey(const QString& cacheKey) const;

signals:
    void databaseInitialized(bool success);
    void metadataChanged();

private:
    explicit ThumbnailDatabase(QObject* parent = nullptr);
    ~ThumbnailDatabase() override;

    bool createTable();
    bool ensureThumbnailDir();

    QSqlDatabase m_db;
    QString m_dbPath;
    QString m_thumbnailDir;
    bool m_initialized = false;
    mutable QMutex m_mutex;

    static ThumbnailDatabase* s_instance;
    static constexpr int DB_VERSION = 1;
};

} // namespace EnhanceVision

#endif // ENHANCEVISION_THUMBNAILDATABASE_H
