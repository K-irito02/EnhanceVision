/**
 * @file ThumbnailProvider.h
 * @brief 缩略图提供者
 * @author Qt客户端开发工程师
 */

#ifndef ENHANCEVISION_THUMBNAILPROVIDER_H
#define ENHANCEVISION_THUMBNAILPROVIDER_H

#include <QQuickImageProvider>
#include <QImage>
#include <QMap>
#include <QSet>
#include <QMutex>
#include <QThreadPool>
#include <QRunnable>
#include <QObject>

namespace EnhanceVision {

/**
 * @brief 异步缩略图生成任务
 */
class ThumbnailGenerator : public QObject, public QRunnable
{
    Q_OBJECT

public:
    explicit ThumbnailGenerator(const QString &filePath, const QString &id, const QSize &size);
    void run() override;

signals:
    void thumbnailReady(const QString &id, const QImage &thumbnail);

private:
    QString m_filePath;
    QString m_id;
    QSize m_size;
};

/**
 * @brief 缩略图提供者，用于提供缩略图给 QML
 */
class ThumbnailProvider : public QQuickImageProvider
{
    Q_OBJECT

public:
    ThumbnailProvider();
    ~ThumbnailProvider() override;

    /**
     * @brief 规范化文件路径
     * @param path 原始路径
     * @return 规范化后的路径
     */
    static QString normalizeFilePath(const QString &path);

    /**
     * @brief 请求图像
     * @param id 图像 ID
     * @param size 原始图像大小
     * @param requestedSize 请求的图像大小
     * @return 请求的图像
     */
    QImage requestImage(const QString &id, QSize *size, const QSize &requestedSize) override;

    /**
     * @brief 设置缩略图
     * @param id 缩略图 ID
     * @param thumbnail 缩略图
     */
    void setThumbnail(const QString &id, const QImage &thumbnail);

    /**
     * @brief 异步生成缩略图
     * @param filePath 文件路径
     * @param id 缩略图 ID
     * @param size 缩略图尺寸
     */
    void generateThumbnailAsync(const QString &filePath, const QString &id, const QSize &size = QSize(256, 256));

    /**
     * @brief 移除缩略图
     * @param id 缩略图 ID
     */
    void removeThumbnail(const QString &id);

    /**
     * @brief 清除所有缩略图
     */
    void clearAll();

    /**
     * @brief 获取单例实例
     * @return 单例实例
     */
    static ThumbnailProvider* instance();

signals:
    /**
     * @brief 缩略图准备就绪信号
     * @param id 缩略图 ID
     */
    void thumbnailReady(const QString &id);

private slots:
    void onThumbnailReady(const QString &id, const QImage &thumbnail);

private:
    QMap<QString, QImage> m_thumbnails;     ///< 缩略图映射
    QSet<QString> m_pendingRequests;        ///< 正在生成中的请求
    QMutex m_mutex;                           ///< 互斥锁，保证线程安全
    QThreadPool* m_threadPool;                ///< 线程池
    static ThumbnailProvider* s_instance;     ///< 单例实例
    
    QImage generatePlaceholderImage(const QSize& size);
};

} // namespace EnhanceVision

#endif // ENHANCEVISION_THUMBNAILPROVIDER_H
