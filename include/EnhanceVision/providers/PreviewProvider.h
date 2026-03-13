/**
 * @file PreviewProvider.h
 * @brief 预览图像提供者
 * @author Qt客户端开发工程师
 */

#ifndef ENHANCEVISION_PREVIEWPROVIDER_H
#define ENHANCEVISION_PREVIEWPROVIDER_H

#include <QQuickImageProvider>
#include <QImage>
#include <QMap>
#include <QMutex>

namespace EnhanceVision {

/**
 * @brief 预览图像提供者，用于提供预览图像给 QML
 */
class PreviewProvider : public QQuickImageProvider
{
public:
    PreviewProvider();
    ~PreviewProvider() override;

    /**
     * @brief 请求图像
     * @param id 图像 ID
     * @param size 原始图像大小
     * @param requestedSize 请求的图像大小
     * @return 请求的图像
     */
    QImage requestImage(const QString &id, QSize *size, const QSize &requestedSize) override;

    /**
     * @brief 设置预览图像
     * @param id 图像 ID
     * @param image 预览图像
     */
    void setPreviewImage(const QString &id, const QImage &image);

    /**
     * @brief 移除预览图像
     * @param id 图像 ID
     */
    void removePreviewImage(const QString &id);

    /**
     * @brief 清除所有预览图像
     */
    void clearAll();

    /**
     * @brief 获取单例实例
     * @return 单例实例
     */
    static PreviewProvider* instance();

private:
    QMap<QString, QImage> m_previewImages;  ///< 预览图像映射
    QMutex m_mutex;                           ///< 互斥锁，保证线程安全
    static PreviewProvider* s_instance;      ///< 单例实例
};

} // namespace EnhanceVision

#endif // ENHANCEVISION_PREVIEWPROVIDER_H
