#include "EnhanceVision/providers/ThumbnailProvider.h"
#include "EnhanceVision/core/ThumbnailDatabase.h"
#include "EnhanceVision/utils/ImageUtils.h"
#include <QUrl>
#include <QFileInfo>
#include <QDir>
#include <QPainter>
#include <QPen>
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
    if (s_instance == this) {
        s_instance = nullptr;
    }
    m_threadPool->waitForDone();
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
        qInfo() << "[ThumbnailProvider] Persistence not available (DB not initialized), using memory-only mode";
        m_persistenceEnabled = false;
        return;
    }

    m_persistenceEnabled = true;

    QList<ThumbnailMeta> validEntries = m_db->getAllValid(kPersistenceLoadLimit, "access_count DESC");
    int loadedCount = 0;

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

            loadedCount++;
        }
    }

    qInfo() << "[ThumbnailProvider] Persistence initialized:" << loadedCount
            << "thumbnails loaded from disk cache";
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

QImage ThumbnailProvider::requestImage(const QString &id, QSize *size, const QSize &requestedSize)
{
    const QString cacheKey = normalizeKey(id);

    // 1. 检查 LRU 内存缓存
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

        // 2. 检查失败记录（带冷却期逻辑）
        if (m_failedKeys.contains(cacheKey)) {
            if (isFailCooledDown(cacheKey)) {
                qInfo() << "[ThumbnailProvider] Fail cooldown passed for:" << cacheKey << "- retrying";
                m_failedKeys.remove(cacheKey);
                if (m_persistenceEnabled && m_db) {
                    m_db->updateStatus(cacheKey, static_cast<int>(ThumbStatus::Pending));
                }
            } else {
                QImage placeholder = generatePlaceholderImage(requestedSize);
                if (size) *size = placeholder.size();
                return placeholder;
            }
        }

        // 3. 持久化模式：检查数据库和磁盘
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
            } else if (meta.isFailed()) {
                FailedEntry fe;
                fe.failedAt = meta.lastAccessed > 0 ? meta.lastAccessed : QDateTime::currentSecsSinceEpoch();
                fe.retryCount = meta.failCount;
                m_failedKeys[cacheKey] = fe;

                QImage placeholder = generatePlaceholderImage(requestedSize);
                if (size) *size = placeholder.size();
                return placeholder;
            }
        }

        // 已在生成中，返回占位图
        if (m_pendingRequests.contains(cacheKey)) {
            QImage placeholder = generatePlaceholderImage(requestedSize);
            if (size) *size = placeholder.size();
            return placeholder;
        }
    }

    // 4. 确定实际文件路径并触发异步生成
    const QSize targetSize = requestedSize.isValid() ? requestedSize : QSize(256, 256);
    QString filePath;

    if (cacheKey.startsWith(QStringLiteral("processed_"))) {
        QMutexLocker locker(&m_mutex);
        if (m_idToPath.contains(cacheKey)) {
            filePath = m_idToPath.value(cacheKey);
        }
    } else {
        filePath = normalizeFilePath(cacheKey);
    }

    if (!filePath.isEmpty()) {
        generateThumbnailAsync(filePath, cacheKey, targetSize);
    }

    QImage placeholder = generatePlaceholderImage(requestedSize);
    if (size) *size = placeholder.size();
    return placeholder;
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

    emit thumbnailReady(key);
}

void ThumbnailProvider::generateThumbnailAsync(const QString &filePath, const QString &id, const QSize &size)
{
    const QString key = normalizeKey(id);

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

    if (m_persistenceEnabled && m_db) {
        m_db->removeMetadata(key);
    }
}

void ThumbnailProvider::clearAll()
{
    QMutexLocker locker(&m_mutex);
    m_thumbnails.clear();
    m_lruOrder.clear();
    m_failedKeys.clear();
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

    qInfo() << "[ThumbnailProvider] Cleared" << keysToRemove.size()
            << "thumbnails for path prefix:" << normalizedPrefix;
}

ThumbnailProvider* ThumbnailProvider::instance()
{
    return s_instance;
}

void ThumbnailProvider::onThumbnailReady(const QString &id, const QImage &thumbnail)
{
    const QString key = normalizeKey(id);

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
    }
}

bool ThumbnailProvider::hasThumbnail(const QString &id) const
{
    const QString key = normalizeKey(id);
    QMutexLocker locker(const_cast<QMutex*>(&m_mutex));
    return m_thumbnails.contains(key);
}

QImage ThumbnailProvider::loadPlaceholderFromResource()
{
    QImage placeholder;
    
    placeholder.load(":/icons/placeholder.png");
    
    if (placeholder.isNull()) {
        qWarning() << "[ThumbnailProvider] Failed to load placeholder from resource, using generated one";
    }
    
    return placeholder;
}

QImage ThumbnailProvider::generatePlaceholderImage(const QSize& size)
{
    QSize placeholderSize = size.isValid() ? size : QSize(256, 256);
    
    QImage customPlaceholder = loadPlaceholderFromResource();
    
    if (!customPlaceholder.isNull()) {
        if (customPlaceholder.size() != placeholderSize) {
            return customPlaceholder.scaled(placeholderSize, Qt::KeepAspectRatioByExpanding, Qt::SmoothTransformation);
        }
        return customPlaceholder;
    }
    
    QImage placeholder(placeholderSize, QImage::Format_ARGB32);
    placeholder.fill(Qt::transparent);
    
    QPainter painter(&placeholder);
    painter.setRenderHint(QPainter::Antialiasing);
    
    QColor bgColor(128, 128, 128, 60);
    painter.fillRect(placeholder.rect(), bgColor);
    
    QPen pen(QColor(128, 128, 128, 120));
    pen.setWidth(1);
    painter.setPen(pen);
    painter.drawRect(placeholder.rect().adjusted(0, 0, -1, -1));
    
    return placeholder;
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

} // namespace EnhanceVision
