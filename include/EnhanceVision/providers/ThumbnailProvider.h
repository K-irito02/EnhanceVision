/**
 * @file ThumbnailProvider.h
 * @brief 缩略图提供者
 * @author Qt客户端开发工程师
 */

#ifndef ENHANCEVISION_THUMBNAILPROVIDER_H
#define ENHANCEVISION_THUMBNAILPROVIDER_H

#include <QQuickImageProvider>
#include <QImage>
#include <QHash>
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
     * @brief 规范化文件路径（URL 解码、原生分隔符、去除 Windows 前缀斜杠）
     */
    static QString normalizeFilePath(const QString &path);

    /**
     * @brief 将任意 id（可能含 processed_ 前缀或原始路径）归一化为唯一缓存键
     */
    static QString normalizeKey(const QString &rawId);

    QImage requestImage(const QString &id, QSize *size, const QSize &requestedSize) override;

    void setThumbnail(const QString &id, const QImage &thumbnail, const QString &filePath = QString());
    void generateThumbnailAsync(const QString &filePath, const QString &id, const QSize &size = QSize(256, 256));
    void removeThumbnail(const QString &id);
    void clearAll();

    /**
     * @brief 使指定 id 的缓存失效并重新生成
     */
    Q_INVOKABLE void invalidateThumbnail(const QString &id);

    /**
     * @brief 检查指定 id 的缩略图是否已在缓存中
     */
    Q_INVOKABLE bool hasThumbnail(const QString &id) const;

    static ThumbnailProvider* instance();

signals:
    /**
     * @brief 缩略图准备就绪信号（id 始终为 normalizeKey 后的值）
     */
    void thumbnailReady(const QString &id);

private slots:
    void onThumbnailReady(const QString &id, const QImage &thumbnail);

private:
    QHash<QString, QImage> m_thumbnails;    ///< normalizedKey → 缩略图
    QHash<QString, QString> m_idToPath;     ///< normalizedKey → 实际文件路径（processed_ 等别名）
    QSet<QString> m_pendingRequests;        ///< 正在生成中的 normalizedKey
    QSet<QString> m_failedKeys;             ///< 生成失败的 normalizedKey（防止无限重试）
    QMutex m_mutex;
    QThreadPool* m_threadPool;
    static ThumbnailProvider* s_instance;

    QImage generatePlaceholderImage(const QSize& size);
};

} // namespace EnhanceVision

#endif // ENHANCEVISION_THUMBNAILPROVIDER_H
