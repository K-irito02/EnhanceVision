/**
 * @file ThumbnailCacheService.cpp
 * @brief 缩略图 LRU 内存缓存实现
 * @author Qt客户端开发工程师
 */

#include "EnhanceVision/services/ThumbnailCacheService.h"
#include <QMutexLocker>
#include <QDebug>

namespace EnhanceVision {

ThumbnailCacheService* ThumbnailCacheService::s_instance = nullptr;

ThumbnailCacheService::ThumbnailCacheService(QObject* parent)
    : QObject(parent)
{
}

ThumbnailCacheService* ThumbnailCacheService::instance()
{
    if (!s_instance) {
        s_instance = new ThumbnailCacheService();
    }
    return s_instance;
}

void ThumbnailCacheService::destroyInstance()
{
    delete s_instance;
    s_instance = nullptr;
}

qint64 ThumbnailCacheService::calculateImageSize(const QImage& image) const
{
    return static_cast<qint64>(image.sizeInBytes());
}

void ThumbnailCacheService::evictIfNeeded()
{
    while ((!m_cacheList.isEmpty() && m_currentCount > m_maxCount) ||
           (m_currentMemoryUsage > m_maxMemoryBytes && !m_cacheList.isEmpty())) {

        CacheEntry entry = m_cacheList.takeFirst();
        m_indexMap.remove(entry.key);
        m_currentMemoryUsage -= entry.sizeBytes;

        emit cacheEvicted(entry.key);
    }
}

QImage ThumbnailCacheService::get(const QString& key)
{
    QMutexLocker locker(&m_mutex);

    if (key.isEmpty() || !m_indexMap.contains(key)) return QImage();

    int idx = m_indexMap.value(key);

    CacheEntry entry = m_cacheList.takeAt(idx);
    m_cacheList.append(entry);

    for (auto it = m_indexMap.begin(); it != m_indexMap.end(); ++it) {
        if (*it > idx) {
            --(*it);
        }
    }
    m_indexMap[key] = m_cacheList.size() - 1;

    return entry.image;
}

void ThumbnailCacheService::put(const QString& key, const QImage& image)
{
    if (key.isEmpty() || image.isNull()) return;

    QMutexLocker locker(&m_mutex);

    qint64 imgSize = calculateImageSize(image);

    if (m_indexMap.contains(key)) {
        int oldIdx = m_indexMap[key];
        CacheEntry& oldEntry = m_cacheList[oldIdx];
        m_currentMemoryUsage -= oldEntry.sizeBytes;

        CacheEntry newEntry{key, image, imgSize};
        m_cacheList.removeAt(oldIdx);
        m_cacheList.append(newEntry);

        for (auto it = m_indexMap.begin(); it != m_indexMap.end(); ++it) {
            if (*it > oldIdx) --(*it);
        }
        m_indexMap[key] = m_cacheList.size() - 1;
    } else {
        CacheEntry entry{key, image, imgSize};
        m_cacheList.append(entry);
        m_indexMap[key] = m_cacheList.size() - 1;
        ++m_currentCount;
    }

    m_currentMemoryUsage += imgSize;

    evictIfNeeded();
}

void ThumbnailCacheService::remove(const QString& key)
{
    QMutexLocker locker(&m_mutex);

    if (!m_indexMap.contains(key)) return;

    int idx = m_indexMap.take(key);
    CacheEntry removed = m_cacheList.takeAt(idx);
    m_currentMemoryUsage -= removed.sizeBytes;
    --m_currentCount;

    for (auto it = m_indexMap.begin(); it != m_indexMap.end(); ++it) {
        if (*it > idx) --(*it);
    }
}

void ThumbnailCacheService::clear()
{
    QMutexLocker locker(&m_mutex);
    m_cacheList.clear();
    m_indexMap.clear();
    m_currentCount = 0;
    m_currentMemoryUsage = 0;
}

bool ThumbnailCacheService::contains(const QString& key) const
{
    QMutexLocker locker(&m_mutex);
    return m_indexMap.contains(key);
}

qint64 ThumbnailCacheService::currentMemoryUsage() const
{
    QMutexLocker locker(&m_mutex);
    return m_currentMemoryUsage;
}

int ThumbnailCacheService::currentCount() const
{
    QMutexLocker locker(&m_mutex);
    return m_currentCount;
}

} // namespace EnhanceVision
