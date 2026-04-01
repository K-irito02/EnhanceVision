/**
 * @file ThumbnailCacheService.h
 * @brief 缩略图 LRU 内存缓存服务（L1 层）
 * @author Qt客户端开发工程师
 */

#ifndef ENHANCEVISION_THUMBNAILCACHESERVICE_H
#define ENHANCEVISION_THUMBNAILCACHESERVICE_H

#include <QObject>
#include <QString>
#include <QImage>
#include <QMutex>
#include <QList>
#include <QHash>

namespace EnhanceVision {

class ThumbnailCacheService : public QObject
{
    Q_OBJECT

public:
    static ThumbnailCacheService* instance();
    static void destroyInstance();

    Q_INVOKABLE QImage get(const QString& key);
    Q_INVOKABLE void put(const QString& key, const QImage& image);
    Q_INVOKABLE void remove(const QString& key);
    Q_INVOKABLE void clear();
    Q_INVOKABLE bool contains(const QString& key) const;

    Q_INVOKABLE qint64 currentMemoryUsage() const;
    Q_INVOKABLE int currentCount() const;

    void setMaxCount(int count) { m_maxCount = count; }
    void setMaxMemoryBytes(qint64 bytes) { m_maxMemoryBytes = bytes; }

signals:
    void cacheEvicted(const QString& key);

private:
    explicit ThumbnailCacheService(QObject* parent = nullptr);
    ~ThumbnailCacheService() override = default;

    void evictIfNeeded();
    qint64 calculateImageSize(const QImage& image) const;

    struct CacheEntry {
        QString key;
        QImage image;
        qint64 sizeBytes = 0;
    };

    QList<CacheEntry> m_cacheList;  // LRU order: front=oldest, back=newest
    QHash<QString, int> m_indexMap; // key -> index in m_cacheList
    mutable QMutex m_mutex;

    int m_maxCount = 500;
    qint64 m_maxMemoryBytes = 200LL * 1024 * 1024;
    qint64 m_currentMemoryUsage = 0;
    int m_currentCount = 0;

    static ThumbnailCacheService* s_instance;
};

} // namespace EnhanceVision

#endif // ENHANCEVISION_THUMBNAILCACHESERVICE_H
