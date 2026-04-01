/**
 * @file ThumbnailProvider.cpp
 * @brief 缩略图提供者实现
 * @author Qt客户端开发工程师
 */

#include "EnhanceVision/providers/ThumbnailProvider.h"
#include "EnhanceVision/utils/ImageUtils.h"
#include <QUrl>
#include <QFileInfo>
#include <QDir>
#include <QPainter>
#include <QPen>

namespace EnhanceVision {

ThumbnailProvider* ThumbnailProvider::s_instance = nullptr;

// ThumbnailGenerator 实现
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

// ThumbnailProvider 实现
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
    
    // 1. 处理 URL 编码的路径
    if (filePath.startsWith("file:///")) {
        filePath = QUrl(filePath).toLocalFile();
    }
    
    // 2. URL 解码（处理 %5C -> \ 等编码）
    if (filePath.contains('%')) {
        filePath = QUrl::fromPercentEncoding(filePath.toUtf8());
    }
    
    // 3. Windows 路径修正：去掉开头的 /（如 /E:/path -> E:/path）
    #ifdef Q_OS_WIN
    if (filePath.startsWith('/') && filePath.length() > 2 && filePath.at(2) == ':') {
        filePath = filePath.mid(1);
    }
    #endif
    
    // 4. 统一路径分隔符为系统原生格式
    filePath = QDir::toNativeSeparators(filePath);
    
    return filePath;
}

QString ThumbnailProvider::normalizeKey(const QString &rawId)
{
    QString key = rawId;
    
    // 1. 去掉查询参数 (?v=N)
    int queryPos = key.indexOf('?');
    if (queryPos > 0) {
        key = key.left(queryPos);
    }
    
    // 2. processed_ 前缀保留原样（它是逻辑 ID，不是文件路径）
    if (key.startsWith(QStringLiteral("processed_"))) {
        return key;
    }
    
    // 3. 对文件路径进行归一化
    return normalizeFilePath(key);
}

QImage ThumbnailProvider::requestImage(const QString &id, QSize *size, const QSize &requestedSize)
{
    const QString cacheKey = normalizeKey(id);

    // 1. 检查缓存
    {
        QMutexLocker locker(&m_mutex);
        if (m_thumbnails.contains(cacheKey)) {
            QImage thumbnail = m_thumbnails.value(cacheKey);
            if (size) {
                *size = thumbnail.size();
            }
            if (!requestedSize.isEmpty() && requestedSize != thumbnail.size()) {
                return ImageUtils::scaleImage(thumbnail, requestedSize, true);
            }
            return thumbnail;
        }

        // 已在生成中或已失败，返回占位图
        if (m_pendingRequests.contains(cacheKey) || m_failedKeys.contains(cacheKey)) {
            QImage placeholder = generatePlaceholderImage(requestedSize);
            if (size) {
                *size = placeholder.size();
            }
            return placeholder;
        }
    }

    // 2. 确定实际文件路径
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

    // 3. 尝试异步生成（内部会检查文件是否存在）
    if (!filePath.isEmpty()) {
        generateThumbnailAsync(filePath, cacheKey, targetSize);
    }

    // 4. 始终返回占位图（不返回空 QImage，避免 QML Image 进入 Error 状态）
    QImage placeholder = generatePlaceholderImage(requestedSize);
    if (size) {
        *size = placeholder.size();
    }
    return placeholder;
}

void ThumbnailProvider::setThumbnail(const QString &id, const QImage &thumbnail, const QString &filePath)
{
    const QString key = normalizeKey(id);
    QMutexLocker locker(&m_mutex);
    m_thumbnails[key] = thumbnail;
    m_failedKeys.remove(key);
    
    if (!filePath.isEmpty()) {
        m_idToPath[key] = filePath;
    }
    
    locker.unlock();
    emit thumbnailReady(key);
}

void ThumbnailProvider::generateThumbnailAsync(const QString &filePath, const QString &id, const QSize &size)
{
    const QString key = normalizeKey(id);

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
    m_failedKeys.remove(key);
}

void ThumbnailProvider::clearAll()
{
    QMutexLocker locker(&m_mutex);
    m_thumbnails.clear();
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
            m_failedKeys.remove(key);
            m_pendingRequests.remove(key);
        }

        for (const QString &key : pathsToRemove) {
            m_idToPath.remove(key);
        }
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
            m_thumbnails[key] = thumbnail;
            m_failedKeys.remove(key);
        } else {
            qWarning() << "[ThumbnailProvider] Thumbnail generation failed for:" << key;
            m_failedKeys.insert(key);
        }
    }
    // 始终发出信号（即使失败），让 QML 有机会处理重试逻辑
    emit thumbnailReady(key);
}

void ThumbnailProvider::invalidateThumbnail(const QString &id)
{
    const QString key = normalizeKey(id);
    QString filePath;

    {
        QMutexLocker locker(&m_mutex);
        m_thumbnails.remove(key);
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

QImage ThumbnailProvider::generatePlaceholderImage(const QSize& size)
{
    QSize placeholderSize = size.isValid() ? size : QSize(256, 256);
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

} // namespace EnhanceVision
