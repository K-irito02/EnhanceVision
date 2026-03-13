/**
 * @file ThumbnailProvider.cpp
 * @brief 缩略图提供者实现
 * @author Qt客户端开发工程师
 */

#include "EnhanceVision/providers/ThumbnailProvider.h"
#include "EnhanceVision/utils/ImageUtils.h"

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

QImage ThumbnailProvider::requestImage(const QString &id, QSize *size, const QSize &requestedSize)
{
    QMutexLocker locker(&m_mutex);
    
    if (!m_thumbnails.contains(id)) {
        return QImage();
    }
    
    QImage thumbnail = m_thumbnails.value(id);
    
    if (size) {
        *size = thumbnail.size();
    }
    
    if (!requestedSize.isEmpty() && requestedSize != thumbnail.size()) {
        return ImageUtils::scaleImage(thumbnail, requestedSize, true);
    }
    
    return thumbnail;
}

void ThumbnailProvider::setThumbnail(const QString &id, const QImage &thumbnail)
{
    QMutexLocker locker(&m_mutex);
    m_thumbnails[id] = thumbnail;
}

void ThumbnailProvider::generateThumbnailAsync(const QString &filePath, const QString &id, const QSize &size)
{
    // 检查是否已经存在缩略图
    {
        QMutexLocker locker(&m_mutex);
        if (m_thumbnails.contains(id)) {
            return;
        }
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
    if (!thumbnail.isNull()) {
        QMutexLocker locker(&m_mutex);
        m_thumbnails[id] = thumbnail;
    }
    emit thumbnailReady(id);
}

} // namespace EnhanceVision
