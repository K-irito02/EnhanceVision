/**
 * @file FileController.cpp
 * @brief 文件控制器实现
 * @author Qt客户端开发工程师
 */

#include "EnhanceVision/controllers/FileController.h"
#include "EnhanceVision/utils/FileUtils.h"
#include "EnhanceVision/utils/ImageUtils.h"
#include <QUuid>
#include <QFileInfo>
#include <QImage>
#include <QDebug>

namespace EnhanceVision {

FileController::FileController(QObject* parent)
    : QObject(parent)
    , m_fileModel(nullptr)
{
    // 初始化支持的文件格式
    m_supportedImageFormats << "jpg" << "jpeg" << "png" << "bmp" << "webp" << "tiff" << "tif";
    m_supportedVideoFormats << "mp4" << "avi" << "mkv" << "mov" << "flv" << "wmv";
}

void FileController::setFileModel(FileModel* model)
{
    if (m_fileModel) {
        disconnect(m_fileModel, nullptr, this, nullptr);
    }
    
    m_fileModel = model;
    
    if (m_fileModel) {
        // 连接 FileModel 信号
        connect(m_fileModel, &FileModel::filesAdded, this, [this](int count) {
            Q_UNUSED(count)
            emit fileCountChanged();
        });
        connect(m_fileModel, &FileModel::fileRemoved, this, [this](int index) {
            Q_UNUSED(index)
            emit fileCountChanged();
        });
        connect(m_fileModel, &FileModel::errorOccurred, this, &FileController::errorOccurred);
    }
}

FileController::~FileController()
{
}

FileModel* FileController::fileModel() const
{
    return m_fileModel;
}

int FileController::fileCount() const
{
    return m_fileModel->rowCount();
}

QStringList FileController::supportedImageFormats() const
{
    return m_supportedImageFormats;
}

QStringList FileController::supportedVideoFormats() const
{
    return m_supportedVideoFormats;
}

void FileController::addFiles(const QList<QUrl>& fileUrls)
{
    QStringList filePaths;
    for (const auto& url : fileUrls) {
        filePaths << url.toLocalFile();
    }
    int addedCount = m_fileModel->addFiles(filePaths);
    if (addedCount > 0) {
        QStringList addedIds;
        for (int i = m_fileModel->rowCount() - addedCount; i < m_fileModel->rowCount(); ++i) {
            addedIds << m_fileModel->fileAt(i).id;
        }
        emit filesAdded(addedIds);
    }
}

void FileController::addFile(const QString& filePath)
{
    if (!isFileSupported(filePath)) {
        emit errorOccurred(tr("不支持的文件格式: %1").arg(filePath));
        return;
    }
    
    if (m_fileModel->addFile(filePath)) {
        const MediaFile& file = m_fileModel->fileAt(m_fileModel->rowCount() - 1);
        emit filesAdded(QStringList() << file.id);
    }
}

void FileController::removeFile(const QString& fileId)
{
    for (int i = 0; i < m_fileModel->rowCount(); ++i) {
        if (m_fileModel->fileAt(i).id == fileId) {
            m_fileModel->removeFile(i);
            emit fileRemoved(fileId);
            break;
        }
    }
}

void FileController::removeFileAt(int index)
{
    if (index >= 0 && index < m_fileModel->rowCount()) {
        QString fileId = m_fileModel->fileAt(index).id;
        m_fileModel->removeFile(index);
        emit fileRemoved(fileId);
    }
}

void FileController::clearFiles()
{
    m_fileModel->clear();
    emit filesCleared();
}

MediaFile* FileController::getFile(const QString& fileId)
{
    // 注意：返回的指针是临时的，不应长期持有
    static MediaFile tempFile;
    for (int i = 0; i < m_fileModel->rowCount(); ++i) {
        if (m_fileModel->fileAt(i).id == fileId) {
            tempFile = m_fileModel->fileAt(i);
            return &tempFile;
        }
    }
    return nullptr;
}

QList<MediaFile> FileController::getSelectedFiles()
{
    QList<MediaFile> selected;
    for (int i = 0; i < m_fileModel->rowCount(); ++i) {
        selected.append(m_fileModel->fileAt(i));
    }
    return selected;
}

QList<MediaFile> FileController::getAllFiles()
{
    QList<MediaFile> allFiles;
    for (int i = 0; i < m_fileModel->rowCount(); ++i) {
        allFiles.append(m_fileModel->fileAt(i));
    }
    return allFiles;
}

void FileController::selectFile(const QString& fileId, bool selected)
{
    // 注意：MediaFile 中暂时没有 isSelected 字段
    // 这里需要后续扩展
    Q_UNUSED(fileId)
    Q_UNUSED(selected)
}

void FileController::selectAllFiles()
{
    // 注意：MediaFile 中暂时没有 isSelected 字段
    // 这里需要后续扩展
}

void FileController::deselectAllFiles()
{
    // 注意：MediaFile 中暂时没有 isSelected 字段
    // 这里需要后续扩展
}

bool FileController::isFileSupported(const QString& filePath) const
{
    QFileInfo fileInfo(filePath);
    QString suffix = fileInfo.suffix().toLower();
    return m_supportedImageFormats.contains(suffix) || m_supportedVideoFormats.contains(suffix);
}

QString FileController::getFileDialogFilter() const
{
    QStringList filters;

    QString imageFilter = tr("图片文件");
    QStringList imagePatterns;
    for (const auto& fmt : m_supportedImageFormats) {
        imagePatterns << QString("*.%1").arg(fmt);
    }
    imageFilter += QString(" (%1)").arg(imagePatterns.join(" "));
    filters << imageFilter;

    QString videoFilter = tr("视频文件");
    QStringList videoPatterns;
    for (const auto& fmt : m_supportedVideoFormats) {
        videoPatterns << QString("*.%1").arg(fmt);
    }
    videoFilter += QString(" (%1)").arg(videoPatterns.join(" "));
    filters << videoFilter;

    QString allFilter = tr("所有支持的文件");
    allFilter += QString(" (%1 %2)").arg(imagePatterns.join(" "), videoPatterns.join(" "));
    filters.prepend(allFilter);

    filters << tr("所有文件 (*.*)");

    return filters.join(";;");
}



} // namespace EnhanceVision
