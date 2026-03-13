/**
 * @file PreviewProvider.cpp
 * @brief 预览图像提供者实现
 * @author Qt客户端开发工程师
 */

#include "EnhanceVision/providers/PreviewProvider.h"
#include "EnhanceVision/utils/ImageUtils.h"

namespace EnhanceVision {

PreviewProvider* PreviewProvider::s_instance = nullptr;

PreviewProvider::PreviewProvider()
    : QQuickImageProvider(QQuickImageProvider::Image)
{
    s_instance = this;
}

PreviewProvider::~PreviewProvider()
{
    if (s_instance == this) {
        s_instance = nullptr;
    }
}

QImage PreviewProvider::requestImage(const QString &id, QSize *size, const QSize &requestedSize)
{
    QMutexLocker locker(&m_mutex);
    
    if (!m_previewImages.contains(id)) {
        return QImage();
    }
    
    QImage image = m_previewImages.value(id);
    
    if (size) {
        *size = image.size();
    }
    
    if (!requestedSize.isEmpty() && requestedSize != image.size()) {
        return ImageUtils::scaleImage(image, requestedSize, true);
    }
    
    return image;
}

void PreviewProvider::setPreviewImage(const QString &id, const QImage &image)
{
    QMutexLocker locker(&m_mutex);
    m_previewImages[id] = image;
}

void PreviewProvider::removePreviewImage(const QString &id)
{
    QMutexLocker locker(&m_mutex);
    m_previewImages.remove(id);
}

void PreviewProvider::clearAll()
{
    QMutexLocker locker(&m_mutex);
    m_previewImages.clear();
}

PreviewProvider* PreviewProvider::instance()
{
    return s_instance;
}

} // namespace EnhanceVision
