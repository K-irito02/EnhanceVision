/**
 * @file ImageUtils.cpp
 * @brief 图像工具类实现
 * @author Qt客户端开发工程师
 */

#include "EnhanceVision/utils/ImageUtils.h"
#include <QFileInfo>
#include <QImageReader>

namespace EnhanceVision {

ImageUtils::ImageUtils(QObject *parent)
    : QObject(parent)
{
}

ImageUtils::~ImageUtils()
{
}

QImage ImageUtils::generateThumbnail(const QString &imagePath, const QSize &size)
{
    QImage image = loadImage(imagePath);
    if (image.isNull()) {
        return QImage();
    }
    return scaleImage(image, size, true);
}

QImage ImageUtils::generateVideoThumbnail(const QString &videoPath, const QSize &size)
{
    Q_UNUSED(videoPath);
    Q_UNUSED(size);
    // TODO: 使用 FFmpeg 从视频提取缩略图
    // 暂时返回空图像，后续完善
    return QImage();
}

QImage ImageUtils::scaleImage(const QImage &image, const QSize &size, bool keepAspectRatio)
{
    if (image.isNull()) {
        return QImage();
    }
    
    QSize targetSize = size;
    if (keepAspectRatio) {
        targetSize = image.size().scaled(size, Qt::KeepAspectRatio);
    }
    
    return image.scaled(targetSize, Qt::KeepAspectRatio, Qt::SmoothTransformation);
}

QImage ImageUtils::loadImage(const QString &path)
{
    QImageReader reader(path);
    reader.setAutoDetectImageFormat(true);
    return reader.read();
}

bool ImageUtils::isImageFile(const QString &filePath)
{
    QFileInfo fileInfo(filePath);
    QString suffix = fileInfo.suffix().toLower();
    
    static const QStringList imageExtensions = {
        QStringLiteral("jpg"), QStringLiteral("jpeg"), 
        QStringLiteral("png"), QStringLiteral("bmp"), 
        QStringLiteral("webp"), QStringLiteral("tiff"),
        QStringLiteral("tif")
    };
    
    return imageExtensions.contains(suffix);
}

bool ImageUtils::isVideoFile(const QString &filePath)
{
    QFileInfo fileInfo(filePath);
    QString suffix = fileInfo.suffix().toLower();
    
    static const QStringList videoExtensions = {
        QStringLiteral("mp4"), QStringLiteral("avi"), 
        QStringLiteral("mkv"), QStringLiteral("mov"), 
        QStringLiteral("flv")
    };
    
    return videoExtensions.contains(suffix);
}

} // namespace EnhanceVision
