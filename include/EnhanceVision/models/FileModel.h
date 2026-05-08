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

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    QHash<int, QByteArray> roleNames() const override;

    Q_INVOKABLE bool addFile(const QString &filePath);
    Q_INVOKABLE int addFiles(const QStringList &filePaths);
    Q_INVOKABLE void removeFile(int index);
    Q_INVOKABLE void clear();
    Q_INVOKABLE bool isFormatSupported(const QString &filePath) const;

    QList<MediaFile> files() const { return m_files; }
    MediaFile fileAt(int index) const;

signals:
    void countChanged();
    void filesAdded(int count);
    void fileRemoved(int index);
    void errorOccurred(const QString &message);
    void thumbnailGenerated(const QString &fileId);

public slots:
    void updateFileThumbnail(const QString &fileId, const QVariant &thumbnailVariant, const QVariant &resolutionVariant);

private:
    QString generateId() const;
    MediaType getMediaType(const QString &filePath) const;
    QString formatFileSize(qint64 size) const;

    QList<MediaFile> m_files;
    QThreadPool* m_threadPool;
    QSet<QString> m_pendingThumbnails;

    int findFileIndexById(const QString &fileId) const;
};

} // namespace EnhanceVision

#endif // ENHANCEVISION_FILEMODEL_H
