/**
 * @file FileModel.cpp
 * @brief 文件列表模型实现
 * @author Qt客户端开发工程师
 */

#include "EnhanceVision/models/FileModel.h"
#include "EnhanceVision/utils/ImageUtils.h"
#include <QUuid>
#include <QFileInfo>
#include <QDir>

namespace EnhanceVision {

// 2GB 最大文件大小
const qint64 FileModel::kMaxFileSize = 2LL * 1024 * 1024 * 1024;

FileModel::FileModel(QObject *parent)
    : QAbstractListModel(parent)
{
}

FileModel::~FileModel()
{
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
        return file.thumbnail;
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

    // 检查文件是否存在
    if (!fileInfo.exists() || !fileInfo.isFile()) {
        emit errorOccurred(tr("文件不存在: %1").arg(filePath));
        return false;
    }

    // 检查文件格式
    if (!isFormatSupported(filePath)) {
        emit errorOccurred(tr("不支持的文件格式: %1").arg(filePath));
        return false;
    }

    // 检查文件大小
    if (!isSizeValid(filePath)) {
        emit errorOccurred(tr("文件大小超过限制 (最大 2GB): %1").arg(filePath));
        return false;
    }

    // 创建 MediaFile 对象
    MediaFile mediaFile;
    mediaFile.id = generateId();
    mediaFile.filePath = QDir::toNativeSeparators(fileInfo.absoluteFilePath());
    mediaFile.fileName = fileInfo.fileName();
    mediaFile.fileSize = fileInfo.size();
    mediaFile.type = getMediaType(filePath);
    mediaFile.status = ProcessingStatus::Pending;
    
    // 生成缩略图
    const QSize thumbnailSize(200, 200);
    if (mediaFile.type == MediaType::Image) {
        mediaFile.thumbnail = ImageUtils::generateThumbnail(filePath, thumbnailSize);
        // 获取图像分辨率
        QImage image = ImageUtils::loadImage(filePath);
        if (!image.isNull()) {
            mediaFile.resolution = image.size();
        }
    } else {
        mediaFile.thumbnail = ImageUtils::generateVideoThumbnail(filePath, thumbnailSize);
    }

    // 添加到列表
    beginInsertRows(QModelIndex(), m_files.size(), m_files.size());
    m_files.append(mediaFile);
    endInsertRows();

    emit countChanged();
    emit filesAdded(1);

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

} // namespace EnhanceVision
