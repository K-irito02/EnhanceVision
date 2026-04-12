#include "EnhanceVision/providers/ThumbnailProvider.h"
#include "EnhanceVision/core/ThumbnailDatabase.h"
#include "EnhanceVision/utils/ImageUtils.h"
#include <QUrl>
#include <QFileInfo>
#include <QDir>
#include <QDateTime>
#include <QImageReader>

namespace EnhanceVision {

ThumbnailProvider* ThumbnailProvider::s_instance = nullptr;

ThumbnailGenerator::ThumbnailGenerator(const QString &filePath, const QString &id, const QSize &size)
    : m_filePath(filePath)
    , m_id(id)
    , m_size(size)
{
    setAutoDelete(true);
}

void ThumbnailGenerator::run()
{
    QImage thumbnail;
    
    if (ImageUtils::isImageFile(m_filePath)) {
        thumbnail = ImageUtils::generateThumbnail(m_filePath, m_size);
    } else if (ImageUtils::isVideoFile(m_filePath)) {
        thumbnail = ImageUtils::generateVideoThumbnail(m_filePath, m_size);
    }
    
    emit thumbnailReady(m_id, thumbnail);
}

ThumbnailProvider::ThumbnailProvider()
    : QQuickImageProvider(QQuickImageProvider::Image)
    , m_threadPool(new QThreadPool(this))
{
    s_instance = this;
    m_threadPool->setMaxThreadCount(4);

    connect(this, &ThumbnailProvider::requestGeneration, this, [this](const QString& path) {
        generateThumbnailAsync(path, path);
    });
}

ThumbnailProvider::~ThumbnailProvider()
{
    prepareForShutdown();

    if (s_instance == this) {
        s_instance = nullptr;
    }
}

QString ThumbnailProvider::normalizeFilePath(const QString &path)
{
    QString filePath = path;
    
    if (filePath.startsWith("file:///")) {
        filePath = QUrl(filePath).toLocalFile();
    }
    
    if (filePath.contains('%')) {
        filePath = QUrl::fromPercentEncoding(filePath.toUtf8());
    }
    
    #ifdef Q_OS_WIN
    if (filePath.startsWith('/') && filePath.length() > 2 && filePath.at(2) == ':') {
        filePath = filePath.mid(1);
    }
    #endif
    
    filePath = QDir::toNativeSeparators(filePath);
    
    return filePath;
}

QString ThumbnailProvider::normalizeKey(const QString &rawId)
{
    QString key = rawId;
    
    int queryPos = key.indexOf('?');
    if (queryPos > 0) {
        key = key.left(queryPos);
    }
    
    if (key.startsWith(QStringLiteral("processed_"))) {
        return key;
    }
    
    return normalizeFilePath(key);
}

void ThumbnailProvider::initializePersistence()
{
    QMutexLocker locker(&m_mutex);

    m_db = ThumbnailDatabase::instance();
    if (!m_db || !m_db->isInitialized()) {
        m_persistenceEnabled = false;
        return;
    }

    m_persistenceEnabled = true;

    QList<ThumbnailMeta> validEntries = m_db->getAllValid(kPersistenceLoadLimit, "access_count DESC");

    for (const auto& meta : validEntries) {
        QImage diskImg = loadThumbnailFromDisk(meta.cacheKey);
        if (!diskImg.isNull()) {
            LRUEntry entry;
            entry.image = diskImg;
            entry.lastAccess = QDateTime::currentSecsSinceEpoch();
            m_thumbnails[meta.cacheKey] = entry;
            m_lruOrder.append(meta.cacheKey);

            if (!meta.filePath.isEmpty()) {
                m_idToPath[meta.cacheKey] = meta.filePath;
            }
        }
    }

}

int ThumbnailProvider::memoryCacheCount() const
{
    QMutexLocker locker(const_cast<QMutex*>(&m_mutex));
    return m_thumbnails.size();
}

qint64 ThumbnailProvider::memoryCacheSize() const
{
    QMutexLocker locker(const_cast<QMutex*>(&m_mutex));
    qint64 totalBytes = 0;
    for (auto it = m_thumbnails.constBegin(); it != m_thumbnails.constEnd(); ++it) {
        totalBytes += it.value().image.sizeInBytes();
    }
    return totalBytes;
}

void ThumbnailProvider::prepareForShutdown(int timeoutMs)
{
    if (m_shutdownInProgress) {
        return;
    }

    m_shutdownInProgress = true;

    {
        QMutexLocker locker(&m_mutex);
        m_pendingRequests.clear();
    }

    if (!m_threadPool) {
        return;
    }

    m_threadPool->clear();
    if (!m_threadPool->waitForDone(timeoutMs)) {
        qWarning() << "[ThumbnailProvider] Thumbnail worker shutdown timed out";
    }
}

QImage ThumbnailProvider::requestImage(const QString &id, QSize *size, const QSize &requestedSize)
{
    const QString cacheKey = normalizeKey(id);
    const QSize targetSize = requestedSize.isValid() ? requestedSize : QSize(256, 256);

    {
        QMutexLocker locker(&m_mutex);
        if (m_thumbnails.contains(cacheKey)) {
            touchLRU(cacheKey);
            LRUEntry& entry = m_thumbnails[cacheKey];
            if (size) {
                *size = entry.image.size();
            }
            if (!requestedSize.isEmpty() && requestedSize != entry.image.size()) {
                return ImageUtils::scaleImage(entry.image, requestedSize, true);
            }

            if (m_persistenceEnabled && m_db) {
                m_db->updateAccessTime(cacheKey);
            }

            return entry.image;
        }

        if (m_failedKeys.contains(cacheKey)) {
            if (isFailCooledDown(cacheKey)) {
                m_failedKeys.remove(cacheKey);
            } else {
                if (size) {
                    *size = targetSize;
                }
                return QImage();
            }
        }

        if (m_persistenceEnabled && m_db) {
            ThumbnailMeta meta = m_db->getMetadata(cacheKey);
            if (meta.isValid() && !meta.diskPath.isEmpty()) {
                QImage diskImg = loadThumbnailFromDisk(cacheKey);
                if (!diskImg.isNull()) {
                    LRUEntry entry;
                    entry.image = diskImg;
                    entry.lastAccess = QDateTime::currentSecsSinceEpoch();
                    m_thumbnails[cacheKey] = entry;
                    promoteToMRU(cacheKey);

                    if (size) *size = diskImg.size();
                    if (!requestedSize.isEmpty() && requestedSize != diskImg.size()) {
                        return ImageUtils::scaleImage(diskImg, requestedSize, true);
                    }
                    evictLRU();
                    return diskImg;
                } else {
                    qWarning() << "[ThumbnailProvider] Disk file missing for valid DB entry:" << cacheKey
                               << "-" << meta.diskPath;
                    m_db->updateStatus(cacheKey, static_cast<int>(ThumbStatus::Stale));
                }
            } else if (!meta.cacheKey.isEmpty() && meta.isFailed()) {
                FailedEntry fe;
                fe.failedAt = meta.lastAccessed > 0 ? meta.lastAccessed : QDateTime::currentSecsSinceEpoch();
                fe.retryCount = meta.failCount;
                m_failedKeys[cacheKey] = fe;

                if (!isFailCooledDown(cacheKey)) {
                    if (size) {
                        *size = targetSize;
                    }
                    return QImage();
                }

                m_failedKeys.remove(cacheKey);
            }
        }

        if (m_pendingRequests.contains(cacheKey)) {
            if (size) {
                *size = targetSize;
            }
            return QImage();
        }
    }

    QString filePath;
    {
        QMutexLocker locker(&m_mutex);
        filePath = resolveFilePathLocked(cacheKey);
    }

    if (!filePath.isEmpty()) {
        generateThumbnailAsync(filePath, cacheKey, targetSize);
    }

    if (size) {
        *size = targetSize;
    }
    return QImage();
}

void ThumbnailProvider::setThumbnail(const QString &id, const QImage &thumbnail, const QString &filePath)
{
    const QString key = normalizeKey(id);
    QMutexLocker locker(&m_mutex);

    LRUEntry entry;
    entry.image = thumbnail;
    entry.lastAccess = QDateTime::currentSecsSinceEpoch();

    m_thumbnails[key] = entry;
    promoteToMRU(key);
    m_failedKeys.remove(key);

    if (!filePath.isEmpty()) {
        m_idToPath[key] = filePath;
    }

    locker.unlock();

    saveThumbnailToDisk(key, thumbnail);
    evictLRU();

    emitThumbnailStateChanged(key, ThumbnailState::Ready);
    emit thumbnailReady(key);
}

void ThumbnailProvider::generateThumbnailAsync(const QString &filePath, const QString &id, const QSize &size)
{
    const QString key = normalizeKey(id);

    if (m_shutdownInProgress) {
        return;
    }

    if (filePath.isEmpty()) {
        qWarning() << "[ThumbnailProvider] generateThumbnailAsync called with empty filePath for id:" << id;
        return;
    }

    {
        QMutexLocker locker(&m_mutex);
        if (m_thumbnails.contains(key) || m_pendingRequests.contains(key)) {
            return;
        }

        QFileInfo fileInfo(filePath);
        if (!fileInfo.exists()) {
            qWarning() << "[ThumbnailProvider] File does not exist, skipping thumbnail generation:" << filePath;
            return;
        }

        if (fileInfo.size() == 0) {
            qWarning() << "[ThumbnailProvider] File is empty, skipping thumbnail generation:" << filePath;
            return;
        }

        m_pendingRequests.insert(key);
        if (key.startsWith("processed_")) {
            m_idToPath[key] = filePath;
        }

        if (m_persistenceEnabled && m_db) {
            ThumbnailMeta meta;
            meta.cacheKey = key;
            meta.filePath = filePath;
            meta.status = static_cast<int>(ThumbStatus::Pending);
            m_db->upsertMetadata(meta);
        }
    }

    emitThumbnailStateChanged(key, ThumbnailState::Pending);

    ThumbnailGenerator* generator = new ThumbnailGenerator(filePath, key, size);
    connect(generator, &ThumbnailGenerator::thumbnailReady,
            this, &ThumbnailProvider::onThumbnailReady);
    m_threadPool->start(generator);
}

void ThumbnailProvider::removeThumbnail(const QString &id)
{
    const QString key = normalizeKey(id);
    QMutexLocker locker(&m_mutex);
    m_thumbnails.remove(key);
    m_lruOrder.removeAll(key);
    m_failedKeys.remove(key);
    m_pendingRequests.remove(key);
    m_idToPath.remove(key);

    if (m_persistenceEnabled && m_db) {
        m_db->removeMetadata(key);
    }

    locker.unlock();
    emitThumbnailStateChanged(key, ThumbnailState::Missing);
}

void ThumbnailProvider::clearAll()
{
    QMutexLocker locker(&m_mutex);
    m_thumbnails.clear();
    m_lruOrder.clear();
    m_failedKeys.clear();
    m_pendingRequests.clear();
    m_idToPath.clear();
}

void ThumbnailProvider::clearThumbnailsByPathPrefix(const QString &pathPrefix)
{
    if (pathPrefix.isEmpty()) {
        return;
    }

    QString normalizedPrefix = normalizeFilePath(pathPrefix);
    QStringList keysToRemove;
    QStringList pathsToRemove;

    {
        QMutexLocker locker(&m_mutex);

        for (auto it = m_thumbnails.constBegin(); it != m_thumbnails.constEnd(); ++it) {
            const QString &key = it.key();

            if (key.startsWith(QStringLiteral("processed_"))) {
                QString filePath = m_idToPath.value(key);
                if (!filePath.isEmpty() && filePath.startsWith(normalizedPrefix, Qt::CaseInsensitive)) {
                    keysToRemove.append(key);
                    pathsToRemove.append(key);
                }
            } else {
                if (key.startsWith(normalizedPrefix, Qt::CaseInsensitive)) {
                    keysToRemove.append(key);
                }
            }
        }

        for (const QString &key : keysToRemove) {
            m_thumbnails.remove(key);
            m_lruOrder.removeAll(key);
            m_failedKeys.remove(key);
            m_pendingRequests.remove(key);
        }

        for (const QString &key : pathsToRemove) {
            m_idToPath.remove(key);
        }
    }

    if (m_persistenceEnabled && m_db) {
        m_db->clearByPathPrefix(normalizedPrefix);
    }

}

ThumbnailProvider* ThumbnailProvider::instance()
{
    return s_instance;
}

void ThumbnailProvider::onThumbnailReady(const QString &id, const QImage &thumbnail)
{
    const QString key = normalizeKey(id);
    ThumbnailState nextState = ThumbnailState::Failed;

    {
        QMutexLocker locker(&m_mutex);
        m_pendingRequests.remove(key);

        if (!thumbnail.isNull()) {
            LRUEntry entry;
            entry.image = thumbnail;
            entry.lastAccess = QDateTime::currentSecsSinceEpoch();
            m_thumbnails[key] = entry;
            promoteToMRU(key);
            m_failedKeys.remove(key);

            locker.unlock();

            saveThumbnailToDisk(key, thumbnail);
            evictLRU();
            nextState = ThumbnailState::Ready;
        } else {
            qWarning() << "[ThumbnailProvider] Thumbnail generation failed for:" << key;

            FailedEntry fe;
            fe.failedAt = QDateTime::currentSecsSinceEpoch();
            if (m_failedKeys.contains(key)) {
                fe.retryCount = m_failedKeys[key].retryCount + 1;
            } else {
                fe.retryCount = 1;
            }
            m_failedKeys[key] = fe;

            if (m_persistenceEnabled && m_db) {
                m_db->incrementFailCount(key, QStringLiteral("Generation returned null image"));
            }
        }
    }

    emitThumbnailStateChanged(key, nextState);
    emit thumbnailReady(key);
}

void ThumbnailProvider::invalidateThumbnail(const QString &id)
{
    const QString key = normalizeKey(id);
    QString filePath;

    {
        QMutexLocker locker(&m_mutex);
        m_thumbnails.remove(key);
        m_lruOrder.removeAll(key);
        m_failedKeys.remove(key);
        m_pendingRequests.remove(key);

        if (m_idToPath.contains(key)) {
            filePath = m_idToPath.value(key);
        } else if (!key.startsWith(QStringLiteral("processed_"))) {
            filePath = normalizeFilePath(key);
        }
    }

    if (!filePath.isEmpty()) {
        generateThumbnailAsync(filePath, key, QSize(256, 256));
    } else {
        emitThumbnailStateChanged(key, ThumbnailState::Missing);
    }
}

bool ThumbnailProvider::hasThumbnail(const QString &id) const
{
    return thumbnailState(id) == static_cast<int>(ThumbnailState::Ready);
}

int ThumbnailProvider::thumbnailState(const QString &id) const
{
    const QString key = normalizeKey(id);
    QMutexLocker locker(const_cast<QMutex*>(&m_mutex));
    return static_cast<int>(thumbnailStateLocked(key));
}

void ThumbnailProvider::ensureThumbnail(const QString &id, int width, int height)
{
    const QString key = normalizeKey(id);
    QString filePath;
    QSize requestedSize = QSize(qMax(1, width), qMax(1, height));

    {
        QMutexLocker locker(&m_mutex);
        ThumbnailState state = thumbnailStateLocked(key);
        if (state == ThumbnailState::Ready || state == ThumbnailState::Pending || key.isEmpty()) {
            return;
        }
        filePath = resolveFilePathLocked(key);
    }

    if (!filePath.isEmpty()) {
        generateThumbnailAsync(filePath, key, requestedSize);
    }
}

void ThumbnailProvider::touchLRU(const QString& key)
{
    m_lruOrder.removeAll(key);
    m_lruOrder.prepend(key);
    if (m_thumbnails.contains(key)) {
        m_thumbnails[key].lastAccess = QDateTime::currentSecsSinceEpoch();
    }
}

void ThumbnailProvider::promoteToMRU(const QString& key)
{
    m_lruOrder.removeAll(key);
    m_lruOrder.prepend(key);
}

void ThumbnailProvider::evictLRU()
{
    while (m_thumbnails.size() > kMaxMemoryCacheSize || memoryCacheSize() > kMaxMemoryBytes) {
        if (m_lruOrder.isEmpty()) break;

        QString lruKey = m_lruOrder.takeLast();
        if (m_thumbnails.contains(lruKey)) {
            m_thumbnails.remove(lruKey);
        }
    }
}

bool ThumbnailProvider::saveThumbnailToDisk(const QString& cacheKey, const QImage& image)
{
    if (!m_persistenceEnabled || !m_db || image.isNull()) return false;

    QString diskPath = m_db->diskPathForKey(cacheKey);
    QDir dir = QFileInfo(diskPath).absoluteDir();
    if (!dir.exists()) dir.mkpath(".");

    bool saved = image.save(diskPath, "PNG");
    if (!saved) {
        qWarning() << "[ThumbnailProvider] Failed to save thumbnail to disk:" << diskPath;
        return false;
    }

    ThumbnailMeta meta;
    meta.cacheKey = cacheKey;
    
    if (m_idToPath.contains(cacheKey)) {
        meta.filePath = m_idToPath.value(cacheKey);
    } else {
        meta.filePath = cacheKey;
    }
    
    meta.diskPath = diskPath;
    meta.width = image.width();
    meta.height = image.height();
    meta.fileSize = QFileInfo(diskPath).size();
    meta.status = static_cast<int>(ThumbStatus::Valid);

    m_db->upsertMetadata(meta);
    return true;
}

QImage ThumbnailProvider::loadThumbnailFromDisk(const QString& cacheKey)
{
    if (!m_persistenceEnabled || !m_db) return QImage();

    QString diskPath = m_db->diskPathForKey(cacheKey);
    if (diskPath.isEmpty() || !QFileInfo::exists(diskPath)) {
        ThumbnailMeta meta = m_db->getMetadata(cacheKey);
        if (!meta.diskPath.isEmpty() && QFileInfo::exists(meta.diskPath)) {
            diskPath = meta.diskPath;
        } else {
            return QImage();
        }
    }

    QImageReader reader(diskPath);
    reader.setAutoDetectImageFormat(true);
    reader.setAllocationLimit(0);
    return reader.read();
}

bool ThumbnailProvider::isFailCooledDown(const QString& cacheKey) const
{
    if (!m_failedKeys.contains(cacheKey)) return true;

    const FailedEntry& fe = m_failedKeys[cacheKey];
    qint64 elapsed = QDateTime::currentSecsSinceEpoch() - fe.failedAt;
    return elapsed >= kFailCooldownSec;
}

ThumbnailProvider::ThumbnailState ThumbnailProvider::thumbnailStateLocked(const QString& cacheKey) const
{
    if (cacheKey.isEmpty()) {
        return ThumbnailState::Missing;
    }

    auto memIt = m_thumbnails.constFind(cacheKey);
    if (memIt != m_thumbnails.constEnd() && !memIt.value().image.isNull()) {
        return ThumbnailState::Ready;
    }

    if (m_pendingRequests.contains(cacheKey)) {
        return ThumbnailState::Pending;
    }

    auto failedIt = m_failedKeys.constFind(cacheKey);
    if (failedIt != m_failedKeys.constEnd()) {
        return isFailCooledDown(cacheKey) ? ThumbnailState::Missing : ThumbnailState::Failed;
    }

    if (!m_persistenceEnabled || !m_db) {
        return ThumbnailState::Missing;
    }

    ThumbnailMeta meta = m_db->getMetadata(cacheKey);
    if (meta.cacheKey.isEmpty()) {
        return ThumbnailState::Missing;
    }

    if (meta.isValid()) {
        const QString diskPath = !meta.diskPath.isEmpty() ? meta.diskPath : m_db->diskPathForKey(cacheKey);
        if (!diskPath.isEmpty() && QFileInfo::exists(diskPath)) {
            return ThumbnailState::Ready;
        }
        return ThumbnailState::Missing;
    }

    if (meta.status == static_cast<int>(ThumbStatus::Pending)) {
        return ThumbnailState::Pending;
    }

    if (meta.isFailed()) {
        const qint64 failedAt = meta.lastAccessed > 0 ? meta.lastAccessed : meta.generatedAt;
        const qint64 elapsed = failedAt > 0 ? (QDateTime::currentSecsSinceEpoch() - failedAt) : (kFailCooldownSec + 1);
        return elapsed >= kFailCooldownSec ? ThumbnailState::Missing : ThumbnailState::Failed;
    }

    return ThumbnailState::Missing;
}

QString ThumbnailProvider::resolveFilePathLocked(const QString& cacheKey) const
{
    if (cacheKey.isEmpty()) {
        return QString();
    }

    QString filePath = m_idToPath.value(cacheKey);
    if (filePath.isEmpty() && m_persistenceEnabled && m_db) {
        ThumbnailMeta meta = m_db->getMetadata(cacheKey);
        filePath = meta.filePath;
    }

    if (filePath.isEmpty() && !cacheKey.startsWith(QStringLiteral("processed_"))) {
        filePath = cacheKey;
    }

    return normalizeFilePath(filePath);
}

void ThumbnailProvider::emitThumbnailStateChanged(const QString& cacheKey, ThumbnailState state)
{
    emit thumbnailStateChanged(cacheKey, static_cast<int>(state));
}

} // namespace EnhanceVision
