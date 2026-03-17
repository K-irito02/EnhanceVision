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
    
    /**
     * @brief 应用 Shader 效果到图像
     * @param image 原始图像
     * @param brightness 亮度 (-1.0 ~ 1.0)
     * @param contrast 对比度 (0.0 ~ 2.0)
     * @param saturation 饱和度 (0.0 ~ 2.0)
     * @param hue 色相 (-0.5 ~ 0.5)
     * @param exposure 曝光 (-1.0 ~ 1.0)
     * @param gamma 伽马 (0.5 ~ 2.0)
     * @param temperature 色温 (-0.5 ~ 0.5)
     * @param tint 色调 (-0.5 ~ 0.5)
     * @param vignette 晕影 (0.0 ~ 1.0)
     * @param highlights 高光 (-1.0 ~ 1.0)
     * @param shadows 阴影 (-1.0 ~ 1.0)
     * @return 处理后的图像
     */
    static QImage applyShaderEffects(const QImage &image, 
                                     float brightness = 0.0f,
                                     float contrast = 1.0f,
                                     float saturation = 1.0f,
                                     float hue = 0.0f,
                                     float exposure = 0.0f,
                                     float gamma = 1.0f,
                                     float temperature = 0.0f,
                                     float tint = 0.0f,
                                     float vignette = 0.0f,
                                     float highlights = 0.0f,
                                     float shadows = 0.0f);

private:
    static void rgbToHsv(float r, float g, float b, float &h, float &s, float &v);
    static void hsvToRgb(float h, float s, float v, float &r, float &g, float &b);
};

} // namespace EnhanceVision

#endif // ENHANCEVISION_IMAGEUTILS_H
