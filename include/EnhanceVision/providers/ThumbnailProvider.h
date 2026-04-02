#ifndef ENHANCEVISION_THUMBNAILPROVIDER_H
#define ENHANCEVISION_THUMBNAILPROVIDER_H

#include <QQuickImageProvider>
#include <QImage>
#include <QHash>
#include <QSet>
#include <QList>
#include <QMutex>
#include <QThreadPool>
#include <QRunnable>
#include <QObject>

namespace EnhanceVision {

class ThumbnailDatabase;

struct FailedEntry {
    qint64 failedAt = 0;
    int retryCount = 0;
};

class ThumbnailGenerator : public QObject, public QRunnable
{
    Q_OBJECT

public:
    explicit ThumbnailGenerator(const QString &filePath, const QString &id, const QSize &size);
    void run() override;

signals:
    void thumbnailReady(const QString &id, const QImage &thumbnail);

private:
    QString m_filePath;
    QString m_id;
    QSize m_size;
};

class ThumbnailProvider : public QQuickImageProvider
{
    Q_OBJECT

public:
    ThumbnailProvider();
    ~ThumbnailProvider() override;

    static QString normalizeFilePath(const QString &path);
    static QString normalizeKey(const QString &rawId);

    QImage requestImage(const QString &id, QSize *size, const QSize &requestedSize) override;

    void setThumbnail(const QString &id, const QImage &thumbnail, const QString &filePath = QString());
    void generateThumbnailAsync(const QString &filePath, const QString &id, const QSize &size = QSize(256, 256));
    void removeThumbnail(const QString &id);
    void clearAll();

    void clearThumbnailsByPathPrefix(const QString &pathPrefix);

    Q_INVOKABLE void invalidateThumbnail(const QString &id);
    Q_INVOKABLE bool hasThumbnail(const QString &id) const;

    Q_INVOKABLE void initializePersistence();
    int memoryCacheCount() const;
    qint64 memoryCacheSize() const;

    static ThumbnailProvider* instance();

signals:
    void thumbnailReady(const QString &id);
    void requestGeneration(const QString& path);

private slots:
    void onThumbnailReady(const QString &id, const QImage &thumbnail);

private:
    struct LRUEntry {
        QImage image;
        qint64 lastAccess = 0;
    };

    QHash<QString, LRUEntry> m_thumbnails;
    QList<QString> m_lruOrder;
    QHash<QString, QString> m_idToPath;
    QSet<QString> m_pendingRequests;
    QMap<QString, FailedEntry> m_failedKeys;
    QMutex m_mutex;
    QThreadPool* m_threadPool;
    ThumbnailDatabase* m_db = nullptr;
    bool m_persistenceEnabled = false;
    static ThumbnailProvider* s_instance;

    static constexpr int kMaxMemoryCacheSize = 200;
    static constexpr qint64 kMaxMemoryBytes = 100 * 1024 * 1024;
    static constexpr int kFailCooldownSec = 300;
    static constexpr int kPersistenceLoadLimit = 80;

    QImage generatePlaceholderImage(const QSize& size);
    QImage loadPlaceholderFromResource();

    void touchLRU(const QString& key);
    void evictLRU();
    bool saveThumbnailToDisk(const QString& cacheKey, const QImage& image);
    QImage loadThumbnailFromDisk(const QString& cacheKey);
    bool isFailCooledDown(const QString& cacheKey) const;
    void promoteToMRU(const QString& key);
};

} // namespace EnhanceVision

#endif // ENHANCEVISION_THUMBNAILPROVIDER_H
