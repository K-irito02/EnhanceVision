/**
 * @file FileModel.h
 * @brief 文件列表模型
 * @author Qt客户端开发工程师
 */

#ifndef ENHANCEVISION_FILEMODEL_H
#define ENHANCEVISION_FILEMODEL_H

#include <QAbstractListModel>
#include <QStringList>
#include <QThreadPool>
#include <QRunnable>
#include <QSet>
#include "EnhanceVision/models/DataTypes.h"

namespace EnhanceVision {

class FileModel : public QAbstractListModel
{
    Q_OBJECT
    Q_PROPERTY(int count READ rowCount NOTIFY countChanged)

public:
    /**
     * @brief 数据角色枚举
     */
    enum Roles {
        IdRole = Qt::UserRole + 1,
        FilePathRole,
        FileNameRole,
        FileSizeRole,
        FileSizeDisplayRole,
        MediaTypeRole,
        ThumbnailRole,
        DurationRole,
        ResolutionRole,
        StatusRole,
        ResultPathRole
    };

    explicit FileModel(QObject *parent = nullptr);
    ~FileModel() override;

    // QAbstractListModel 接口实现
    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    QHash<int, QByteArray> roleNames() const override;

    /**
     * @brief 添加单个文件
     * @param filePath 文件路径
     * @return 是否添加成功
     */
    Q_INVOKABLE bool addFile(const QString &filePath);

    /**
     * @brief 添加多个文件
     * @param filePaths 文件路径列表
     * @return 成功添加的文件数量
     */
    Q_INVOKABLE int addFiles(const QStringList &filePaths);

    /**
     * @brief 删除指定索引的文件
     * @param index 文件索引
     */
    Q_INVOKABLE void removeFile(int index);

    /**
     * @brief 清空所有文件
     */
    Q_INVOKABLE void clear();

    /**
     * @brief 验证文件格式是否支持
     * @param filePath 文件路径
     * @return 是否支持
     */
    Q_INVOKABLE bool isFormatSupported(const QString &filePath) const;

    /**
     * @brief 验证文件大小是否在限制范围内
     * @param filePath 文件路径
     * @return 是否符合要求
     */
    Q_INVOKABLE bool isSizeValid(const QString &filePath) const;

    /**
     * @brief 获取文件列表
     * @return 文件列表
     */
    QList<MediaFile> files() const { return m_files; }

    /**
     * @brief 获取指定索引的文件
     * @param index 索引
     * @return 媒体文件对象
     */
    MediaFile fileAt(int index) const;

signals:
    /**
     * @brief 文件数量变化信号
     */
    void countChanged();

    /**
     * @brief 文件添加成功信号
     * @param count 添加的文件数量
     */
    void filesAdded(int count);

    /**
     * @brief 文件删除信号
     * @param index 删除的文件索引
     */
    void fileRemoved(int index);

    /**
     * @brief 错误信号
     * @param message 错误信息
     */
    void errorOccurred(const QString &message);

    /**
     * @brief 缩略图生成完成信号
     * @param fileId 文件ID
     */
    void thumbnailGenerated(const QString &fileId);

public slots:
    /**
     * @brief 更新文件缩略图（由异步任务调用）
     * @param fileId 文件ID
     * @param thumbnailVariant 缩略图（QVariant包装）
     * @param resolutionVariant 分辨率（QVariant包装）
     */
    void updateFileThumbnail(const QString &fileId, const QVariant &thumbnailVariant, const QVariant &resolutionVariant);

private:
    /**
     * @brief 生成唯一ID
     * @return 唯一ID
     */
    QString generateId() const;

    /**
     * @brief 从文件路径提取媒体类型
     * @param filePath 文件路径
     * @return 媒体类型
     */
    MediaType getMediaType(const QString &filePath) const;

    /**
     * @brief 格式化文件大小显示
     * @param size 文件大小（字节）
     * @return 格式化后的字符串
     */
    QString formatFileSize(qint64 size) const;

    QList<MediaFile> m_files;  ///< 文件列表
    static const qint64 kMaxFileSize;  ///< 最大文件大小（2GB）
    
    QThreadPool* m_threadPool;  ///< 线程池用于异步生成缩略图
    QSet<QString> m_pendingThumbnails;  ///< 正在生成缩略图的文件ID
    
    int findFileIndexById(const QString &fileId) const;
};

} // namespace EnhanceVision

#endif // ENHANCEVISION_FILEMODEL_H
