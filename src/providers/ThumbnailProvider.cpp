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

QImage ThumbnailProvider::requestImage(const QString &id, QSize *size, const QSize &requestedSize)
{
    QString cacheKey = id;
    
    int queryPos = cacheKey.indexOf('?');
    if (queryPos > 0) {
        cacheKey = cacheKey.left(queryPos);
    }

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

        if (m_pendingRequests.contains(cacheKey)) {
            QImage placeholder = generatePlaceholderImage(requestedSize);
            if (size) {
                *size = placeholder.size();
            }
            return placeholder;
        }
    }

    QString filePath = normalizeFilePath(cacheKey);
    QSize targetSize = requestedSize.isValid() ? requestedSize : QSize(256, 256);

    if (cacheKey.startsWith("processed_")) {
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
        }

        QImage placeholder = generatePlaceholderImage(requestedSize);
        if (size) {
            *size = placeholder.size();
        }
        return placeholder;
    }

    QFileInfo fileInfo(filePath);
    if (!fileInfo.exists()) {
        qWarning() << "[ThumbnailProvider] File not found:" << filePath << "(from id:" << id << ")";
        if (size) {
            *size = QSize(0, 0);
        }
        return QImage();
    }

    generateThumbnailAsync(filePath, cacheKey, targetSize);

    QImage placeholder = generatePlaceholderImage(requestedSize);
    if (size) {
        *size = placeholder.size();
    }
    return placeholder;
}

void ThumbnailProvider::setThumbnail(const QString &id, const QImage &thumbnail)
{
    QMutexLocker locker(&m_mutex);
    m_thumbnails[id] = thumbnail;
    locker.unlock();
    emit thumbnailReady(id);
}

void ThumbnailProvider::generateThumbnailAsync(const QString &filePath, const QString &id, const QSize &size)
{
    // 检查是否已经存在缩略图或正在生成
    {
        QMutexLocker locker(&m_mutex);
        if (m_thumbnails.contains(id) || m_pendingRequests.contains(id)) {
            return;
        }
        m_pendingRequests.insert(id);
    }

    ThumbnailGenerator* generator = new ThumbnailGenerator(filePath, id, size);
    connect(generator, &ThumbnailGenerator::thumbnailReady,
            this, &ThumbnailProvider::onThumbnailReady);
    m_threadPool->start(generator);
}

void ThumbnailProvider::removeThumbnail(const QString &id)
{
    QMutexLocker locker(&m_mutex);
    m_thumbnails.remove(id);
}

void ThumbnailProvider::clearAll()
{
    QMutexLocker locker(&m_mutex);
    m_thumbnails.clear();
}

ThumbnailProvider* ThumbnailProvider::instance()
{
    return s_instance;
}

void ThumbnailProvider::onThumbnailReady(const QString &id, const QImage &thumbnail)
{
    QString decodedId = id;
    if (id.contains('%')) {
        decodedId = QUrl::fromPercentEncoding(id.toUtf8());
    }
    
    {
        QMutexLocker locker(&m_mutex);
        m_pendingRequests.remove(id);
        if (!thumbnail.isNull()) {
            m_thumbnails[id] = thumbnail;
            if (decodedId != id) {
                m_thumbnails[decodedId] = thumbnail;
            }
        } else {
            qWarning() << "[ThumbnailProvider] Thumbnail generation failed for:" << id;
        }
    }
    emit thumbnailReady(decodedId);
    // 只发出解码后的 ID 信号，避免 QML 收到两次通知导致 thumbVersion 多次递增
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
