/**
 * @file ThumbnailProvider.cpp
 * @brief 缩略图提供者实现
 * @author Qt客户端开发工程师
 */

#include "EnhanceVision/providers/ThumbnailProvider.h"
#include "EnhanceVision/utils/ImageUtils.h"
#include <QUrl>
#include <QFileInfo>

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
    // 先检查缓存
    {
        QMutexLocker locker(&m_mutex);
        if (m_thumbnails.contains(id)) {
            QImage thumbnail = m_thumbnails.value(id);
            if (size) {
                *size = thumbnail.size();
            }
            if (!requestedSize.isEmpty() && requestedSize != thumbnail.size()) {
                return ImageUtils::scaleImage(thumbnail, requestedSize, true);
            }
            return thumbnail;
        }
    }

    // 缓存未命中：id 可能是文件路径，尝试同步生成缩略图
    // 将 URL 编码的路径还原（QML 传入的 id 可能带有 URL 前缀）
    QString filePath = id;
    if (filePath.startsWith("file:///")) {
        filePath = QUrl(filePath).toLocalFile();
    }
    // Windows 路径修正：去掉开头的 /（如 /E:/path -> E:/path）
    #ifdef Q_OS_WIN
    if (filePath.startsWith('/') && filePath.length() > 2 && filePath.at(2) == ':') {
        filePath = filePath.mid(1);
    }
    #endif

    QSize targetSize = requestedSize.isValid() ? requestedSize : QSize(256, 256);
    QImage thumbnail;

    if (ImageUtils::isImageFile(filePath)) {
        thumbnail = ImageUtils::generateThumbnail(filePath, targetSize);
    } else if (ImageUtils::isVideoFile(filePath)) {
        thumbnail = ImageUtils::generateVideoThumbnail(filePath, targetSize);
    }

    if (!thumbnail.isNull()) {
        // 缓存生成的缩略图
        QMutexLocker locker(&m_mutex);
        m_thumbnails[id] = thumbnail;

        if (size) {
            *size = thumbnail.size();
        }
        return thumbnail;
    }

    // 无法生成缩略图
    if (size) {
        *size = QSize(0, 0);
    }
    return QImage();
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
