/**
 * @file FileController.cpp
 * @brief 文件控制器实现
 * @author Qt客户端开发工程师
 */

#include "EnhanceVision/controllers/FileController.h"
#include "EnhanceVision/controllers/SettingsController.h"
#include "EnhanceVision/utils/FileUtils.h"
#include "EnhanceVision/utils/ImageUtils.h"
#include "EnhanceVision/utils/SupportedFormats.h"
#include <QUuid>
#include <QFileInfo>
#include <QDir>
#include <QImage>
#include <QDebug>

namespace EnhanceVision {

FileController::FileController(QObject* parent)
    : QObject(parent)
    , m_fileModel(nullptr)
{
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
    return SupportedFormats::imageExtensions();
}

QStringList FileController::supportedVideoFormats() const
{
    return SupportedFormats::videoExtensions();
}

void FileController::addFiles(const QList<QUrl>& fileUrls)
{
    if (!m_fileModel) {
        emit errorOccurred(tr("文件模型未初始化"));
        return;
    }

    QStringList filePaths;
    filePaths.reserve(fileUrls.size());
    for (const auto& url : fileUrls) {
        if (!url.isValid()) {
            qWarning() << "[FileController] Ignore invalid dropped URL";
            continue;
        }

        if (!url.isLocalFile()) {
            qWarning() << "[FileController] Ignore non-local dropped URL:" << url;
            continue;
        }

        const QString localPath = url.toLocalFile().trimmed();
        if (localPath.isEmpty()) {
            qWarning() << "[FileController] Ignore empty dropped local path";
            continue;
        }

        filePaths << localPath;
    }

    filePaths.removeDuplicates();
    if (filePaths.isEmpty()) {
        qWarning() << "[FileController] No valid local files from dropped URLs";
        return;
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
    return SupportedFormats::isFormatSupported(filePath);
}

QString FileController::getFileDialogFilter() const
{
    QStringList filters;
    filters << SupportedFormats::allSupportedFileDialogFilter();
    filters << SupportedFormats::imageFileDialogFilter();
    filters << SupportedFormats::videoFileDialogFilter();
    filters << SupportedFormats::allFilesDialogFilter();
    return filters.join(";;");
}

QString FileController::saveFileTo(const QString& filePath, const QString& targetDir)
{
    QString dir = targetDir;
    if (dir.isEmpty()) {
        dir = getDefaultSavePath();
    }

    QString savedPath = FileUtils::copyFileTo(filePath, dir);
    if (savedPath.isEmpty()) {
        emit errorOccurred(tr("保存文件失败: %1").arg(filePath));
    } else {
        emit fileSaved(savedPath);
    }
    return savedPath;
}

int FileController::downloadCompletedFiles(const QStringList& resultPaths, const QString& targetDir)
{
    QString dir = targetDir;
    if (dir.isEmpty()) {
        dir = getDefaultSavePath();
    }

    int count = FileUtils::copyFilesTo(resultPaths, dir);
    if (count > 0) {
        // 打开保存目录
        FileUtils::openInExplorer(QDir(dir).filePath("."));
    }
    return count;
}

void FileController::openFileLocation(const QString& filePath)
{
    FileUtils::openInExplorer(filePath);
}

QString FileController::getDefaultSavePath() const
{
    // 优先使用设置中的路径
    QString settingsPath = SettingsController::instance()->defaultSavePath();
    if (!settingsPath.isEmpty()) {
        FileUtils::ensureDirectory(settingsPath);
        return settingsPath;
    }
    return FileUtils::getDefaultSavePath();
}

} // namespace EnhanceVision
