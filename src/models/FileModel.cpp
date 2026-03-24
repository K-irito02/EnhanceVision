/**
 * @file FileModel.cpp
 * @brief 文件列表模型实现
 * @author Qt客户端开发工程师
 */

#include "EnhanceVision/models/FileModel.h"
#include "EnhanceVision/providers/ThumbnailProvider.h"
#include "EnhanceVision/utils/ImageUtils.h"
#include <QUuid>
#include <QFileInfo>
#include <QDir>
#include <QtConcurrent>
#include <QMetaObject>

namespace EnhanceVision {

const qint64 FileModel::kMaxFileSize = 2LL * 1024 * 1024 * 1024;

class ThumbnailGenerationTask : public QRunnable
{
public:
    ThumbnailGenerationTask(const QString &fileId, const QString &filePath, MediaType type, FileModel *model)
        : m_fileId(fileId)
        , m_filePath(filePath)
        , m_type(type)
        , m_model(model)
    {
        setAutoDelete(true);
    }
    
    void run() override
    {
        const QSize thumbnailSize(200, 200);
        QImage thumbnail;
        QSize resolution;
        
        if (m_type == MediaType::Image) {
            thumbnail = ImageUtils::generateThumbnail(m_filePath, thumbnailSize);
            QImage image = ImageUtils::loadImage(m_filePath);
            if (!image.isNull()) {
                resolution = image.size();
            }
        } else {
            thumbnail = ImageUtils::generateVideoThumbnail(m_filePath, thumbnailSize);
        }
        
        if (!thumbnail.isNull() && m_model) {
            QMetaObject::invokeMethod(m_model, "updateFileThumbnail", 
                Qt::QueuedConnection,
                Q_ARG(QString, m_fileId),
                Q_ARG(QVariant, QVariant::fromValue(thumbnail)),
                Q_ARG(QVariant, QVariant::fromValue(resolution)));
        }
    }
    
private:
    QString m_fileId;
    QString m_filePath;
    MediaType m_type;
    FileModel *m_model;
};

FileModel::FileModel(QObject *parent)
    : QAbstractListModel(parent)
    , m_threadPool(new QThreadPool(this))
{
    m_threadPool->setMaxThreadCount(2);
}

FileModel::~FileModel()
{
    m_threadPool->waitForDone(1000);
}

int FileModel::rowCount(const QModelIndex &parent) const
{
    Q_UNUSED(parent)
    return m_files.size();
}

QVariant FileModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= m_files.size()) {
        return QVariant();
    }

    const MediaFile &file = m_files.at(index.row());

    switch (static_cast<Roles>(role)) {
    case IdRole:
        return file.id;
    case FilePathRole:
        return file.filePath;
    case FileNameRole:
        return file.fileName;
    case FileSizeRole:
        return QVariant::fromValue(file.fileSize);
    case FileSizeDisplayRole:
        return formatFileSize(file.fileSize);
    case MediaTypeRole:
        return static_cast<int>(file.type);
    case ThumbnailRole:
        // 返回 ThumbnailProvider URL，QML Image.source 可直接使用
        return file.filePath.isEmpty() ? QString()
            : QStringLiteral("image://thumbnail/") + file.filePath;
    case DurationRole:
        return QVariant::fromValue(file.duration);
    case ResolutionRole:
        return file.resolution;
    case StatusRole:
        return static_cast<int>(file.status);
    case ResultPathRole:
        return file.resultPath;
    default:
        return QVariant();
    }
}

QHash<int, QByteArray> FileModel::roleNames() const
{
    QHash<int, QByteArray> roles;
    roles[IdRole] = "id";
    roles[FilePathRole] = "filePath";
    roles[FileNameRole] = "fileName";
    roles[FileSizeRole] = "fileSize";
    roles[FileSizeDisplayRole] = "fileSizeDisplay";
    roles[MediaTypeRole] = "mediaType";
    roles[ThumbnailRole] = "thumbnail";
    roles[DurationRole] = "duration";
    roles[ResolutionRole] = "resolution";
    roles[StatusRole] = "status";
    roles[ResultPathRole] = "resultPath";
    return roles;
}

bool FileModel::addFile(const QString &filePath)
{
    QFileInfo fileInfo(filePath);

    if (!fileInfo.exists() || !fileInfo.isFile()) {
        emit errorOccurred(tr("文件不存在: %1").arg(filePath));
        return false;
    }

    if (!isFormatSupported(filePath)) {
        emit errorOccurred(tr("不支持的文件格式: %1").arg(filePath));
        return false;
    }

    if (!isSizeValid(filePath)) {
        emit errorOccurred(tr("文件大小超过限制 (最大 2GB): %1").arg(filePath));
        return false;
    }

    MediaFile mediaFile;
    mediaFile.id = generateId();
    mediaFile.filePath = QDir::toNativeSeparators(fileInfo.absoluteFilePath());
    mediaFile.fileName = fileInfo.fileName();
    mediaFile.fileSize = fileInfo.size();
    mediaFile.type = getMediaType(filePath);
    mediaFile.status = ProcessingStatus::Pending;

    beginInsertRows(QModelIndex(), m_files.size(), m_files.size());
    m_files.append(mediaFile);
    endInsertRows();

    emit countChanged();
    emit filesAdded(1);

    if (!m_pendingThumbnails.contains(mediaFile.id)) {
        m_pendingThumbnails.insert(mediaFile.id);
        ThumbnailGenerationTask* task = new ThumbnailGenerationTask(
            mediaFile.id, mediaFile.filePath, mediaFile.type, this);
        m_threadPool->start(task);
    }

    return true;
}

int FileModel::addFiles(const QStringList &filePaths)
{
    int successCount = 0;
    for (const QString &filePath : filePaths) {
        if (addFile(filePath)) {
            successCount++;
        }
    }
    return successCount;
}

void FileModel::removeFile(int index)
{
    if (index < 0 || index >= m_files.size()) {
        return;
    }

    beginRemoveRows(QModelIndex(), index, index);
    m_files.removeAt(index);
    endRemoveRows();

    emit countChanged();
    emit fileRemoved(index);
}

void FileModel::clear()
{
    if (m_files.isEmpty()) {
        return;
    }

    beginResetModel();
    m_files.clear();
    endResetModel();

    emit countChanged();
}

bool FileModel::isFormatSupported(const QString &filePath) const
{
    QFileInfo fileInfo(filePath);
    QString suffix = fileInfo.suffix().toLower();

    // 支持的图片格式
    static const QStringList imageFormats = {"jpg", "jpeg", "png", "bmp", "webp", "tiff", "tif"};
    // 支持的视频格式
    static const QStringList videoFormats = {"mp4", "avi", "mkv", "mov", "flv"};

    return imageFormats.contains(suffix) || videoFormats.contains(suffix);
}

bool FileModel::isSizeValid(const QString &filePath) const
{
    QFileInfo fileInfo(filePath);
    return fileInfo.size() <= kMaxFileSize;
}

MediaFile FileModel::fileAt(int index) const
{
    if (index >= 0 && index < m_files.size()) {
        return m_files.at(index);
    }
    return MediaFile();
}

QString FileModel::generateId() const
{
    return QUuid::createUuid().toString(QUuid::WithoutBraces);
}

MediaType FileModel::getMediaType(const QString &filePath) const
{
    QFileInfo fileInfo(filePath);
    QString suffix = fileInfo.suffix().toLower();

    static const QStringList imageFormats = {"jpg", "jpeg", "png", "bmp", "webp", "tiff", "tif"};

    if (imageFormats.contains(suffix)) {
        return MediaType::Image;
    }
    return MediaType::Video;
}

QString FileModel::formatFileSize(qint64 size) const
{
    if (size < 1024) {
        return tr("%1 B").arg(size);
    } else if (size < 1024 * 1024) {
        return tr("%1 KB").arg(size / 1024.0, 0, 'f', 2);
    } else if (size < 1024 * 1024 * 1024) {
        return tr("%1 MB").arg(size / (1024.0 * 1024.0), 0, 'f', 2);
    } else {
        return tr("%1 GB").arg(size / (1024.0 * 1024.0 * 1024.0), 0, 'f', 2);
    }
}

void FileModel::updateFileThumbnail(const QString &fileId, const QVariant &thumbnailVariant, const QVariant &resolutionVariant)
{
    m_pendingThumbnails.remove(fileId);
    
    int index = findFileIndexById(fileId);
    if (index < 0) {
        return;
    }
    
    QImage thumbnail = thumbnailVariant.value<QImage>();
    QSize resolution = resolutionVariant.toSize();
    
    m_files[index].thumbnail = thumbnail;
    if (resolution.isValid()) {
        m_files[index].resolution = resolution;
    }
    
    if (!thumbnail.isNull()) {
        if (auto* provider = ThumbnailProvider::instance()) {
            provider->setThumbnail(m_files[index].filePath, thumbnail);
        }
    }
    
    QModelIndex modelIndex = createIndex(index, 0);
    emit dataChanged(modelIndex, modelIndex, {ThumbnailRole, ResolutionRole});
    emit thumbnailGenerated(fileId);
}

int FileModel::findFileIndexById(const QString &fileId) const
{
    for (int i = 0; i < m_files.size(); ++i) {
        if (m_files[i].id == fileId) {
            return i;
        }
    }
    return -1;
}

} // namespace EnhanceVision
