/**
 * @file ImageUtils.h
 * @brief 图像工具类
 * @author Qt客户端开发工程师
 */

#ifndef ENHANCEVISION_IMAGEUTILS_H
#define ENHANCEVISION_IMAGEUTILS_H

#include <QObject>
#include <QImage>
#include <QString>
#include <QSize>

namespace EnhanceVision {

/**
 * @brief 图像工具类，提供图像处理相关功能
 */
class ImageUtils : public QObject
{
    Q_OBJECT

public:
    explicit ImageUtils(QObject *parent = nullptr);
    ~ImageUtils() override;

    /**
     * @brief 生成缩略图
     * @param imagePath 图像路径
     * @param size 缩略图尺寸
     * @return 生成的缩略图
     */
    static QImage generateThumbnail(const QString &imagePath, const QSize &size);

    /**
     * @brief 从视频生成缩略图
     * @param videoPath 视频路径
     * @param size 缩略图尺寸
     * @return 生成的缩略图
     */
    static QImage generateVideoThumbnail(const QString &videoPath, const QSize &size);

    /**
     * @brief 缩放图像
     * @param image 原始图像
     * @param size 目标尺寸
     * @param keepAspectRatio 是否保持宽高比
     * @return 缩放后的图像
     */
    static QImage scaleImage(const QImage &image, const QSize &size, bool keepAspectRatio = true);

    /**
     * @brief 加载图像
     * @param path 图像路径
     * @return 加载的图像
     */
    static QImage loadImage(const QString &path);

    /**
     * @brief 判断文件是否为图像
     * @param filePath 文件路径
     * @return 是否为图像
     */
    static bool isImageFile(const QString &filePath);

    /**
     * @brief 判断文件是否为视频
     * @param filePath 文件路径
     * @return 是否为视频
     */
    static bool isVideoFile(const QString &filePath);
};

} // namespace EnhanceVision

#endif // ENHANCEVISION_IMAGEUTILS_H
