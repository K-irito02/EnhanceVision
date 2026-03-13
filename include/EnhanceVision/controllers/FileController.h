/**
 * @file FileController.h
 * @brief 文件控制器 - 管理媒体文件导入、预览、删除等
 * @author Qt客户端开发工程师
 */

#ifndef ENHANCEVISION_FILECONTROLLER_H
#define ENHANCEVISION_FILECONTROLLER_H

#include <QObject>
#include <QString>
#include <QStringList>
#include <QUrl>
#include "EnhanceVision/models/DataTypes.h"
#include "EnhanceVision/models/FileModel.h"

namespace EnhanceVision {

/**
 * @brief 文件控制器
 */
class FileController : public QObject
{
    Q_OBJECT
    Q_PROPERTY(FileModel* fileModel READ fileModel CONSTANT)
    Q_PROPERTY(int fileCount READ fileCount NOTIFY fileCountChanged)
    Q_PROPERTY(QStringList supportedImageFormats READ supportedImageFormats CONSTANT)
    Q_PROPERTY(QStringList supportedVideoFormats READ supportedVideoFormats CONSTANT)

public:
    explicit FileController(QObject* parent = nullptr);
    ~FileController() override;

    // 属性访问器
    FileModel* fileModel() const;
    int fileCount() const;
    QStringList supportedImageFormats() const;
    QStringList supportedVideoFormats() const;

    // 设置 FileModel（用于 Application 中统一管理）
    void setFileModel(FileModel* model);

    // Q_INVOKABLE 方法
    Q_INVOKABLE void addFiles(const QList<QUrl>& fileUrls);
    Q_INVOKABLE void addFile(const QString& filePath);
    Q_INVOKABLE void removeFile(const QString& fileId);
    Q_INVOKABLE void removeFileAt(int index);
    Q_INVOKABLE void clearFiles();
    Q_INVOKABLE MediaFile* getFile(const QString& fileId);
    Q_INVOKABLE QList<MediaFile> getSelectedFiles();
    Q_INVOKABLE QList<MediaFile> getAllFiles();
    Q_INVOKABLE void selectFile(const QString& fileId, bool selected);
    Q_INVOKABLE void selectAllFiles();
    Q_INVOKABLE void deselectAllFiles();
    Q_INVOKABLE bool isFileSupported(const QString& filePath) const;
    Q_INVOKABLE QString getFileDialogFilter() const;

signals:
    void fileCountChanged();
    void filesAdded(const QStringList& fileIds);
    void fileRemoved(const QString& fileId);
    void filesCleared();
    void fileSelected(const QString& fileId, bool selected);
    void errorOccurred(const QString& error);

private:
    FileModel* m_fileModel;
    QStringList m_supportedImageFormats;
    QStringList m_supportedVideoFormats;
};

} // namespace EnhanceVision

#endif // ENHANCEVISION_FILECONTROLLER_H
