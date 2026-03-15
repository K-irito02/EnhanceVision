/**
 * @file FileUtils.cpp
 * @brief 文件工具类实现
 * @author Qt客户端开发工程师
 */

#include "EnhanceVision/utils/FileUtils.h"
#include <QFile>
#include <QFileInfo>
#include <QDir>
#include <QDateTime>
#include <QStandardPaths>
#include <QProcess>
#include <QDebug>

namespace EnhanceVision {

FileUtils::FileUtils(QObject *parent)
    : QObject(parent)
{
}

FileUtils::~FileUtils()
{
}

QString FileUtils::copyFileTo(const QString &sourcePath, const QString &targetDir, bool overwrite)
{
    QFileInfo sourceInfo(sourcePath);
    if (!sourceInfo.exists() || !sourceInfo.isFile()) {
        qWarning() << "FileUtils::copyFileTo: source file does not exist:" << sourcePath;
        return QString();
    }

    if (!ensureDirectory(targetDir)) {
        qWarning() << "FileUtils::copyFileTo: cannot create target directory:" << targetDir;
        return QString();
    }

    QString targetFileName = overwrite
        ? sourceInfo.fileName()
        : generateUniqueFileName(targetDir, sourceInfo.fileName());
    QString targetPath = QDir(targetDir).filePath(targetFileName);

    if (overwrite && QFile::exists(targetPath)) {
        QFile::remove(targetPath);
    }

    if (QFile::copy(sourcePath, targetPath)) {
        return targetPath;
    }

    qWarning() << "FileUtils::copyFileTo: failed to copy" << sourcePath << "to" << targetPath;
    return QString();
}

int FileUtils::copyFilesTo(const QStringList &sourcePaths, const QString &targetDir)
{
    int successCount = 0;
    for (const QString &path : sourcePaths) {
        if (!copyFileTo(path, targetDir).isEmpty()) {
            successCount++;
        }
    }
    return successCount;
}

bool FileUtils::ensureDirectory(const QString &dirPath)
{
    QDir dir(dirPath);
    if (dir.exists()) {
        return true;
    }
    return dir.mkpath(".");
}

QString FileUtils::generateUniqueFileName(const QString &dirPath, const QString &fileName)
{
    QDir dir(dirPath);
    if (!dir.exists(fileName)) {
        return fileName;
    }

    QFileInfo fi(fileName);
    QString baseName = fi.completeBaseName();
    QString suffix = fi.suffix();

    for (int i = 1; i < 10000; ++i) {
        QString newName = suffix.isEmpty()
            ? QStringLiteral("%1_%2").arg(baseName).arg(i)
            : QStringLiteral("%1_%2.%3").arg(baseName).arg(i).arg(suffix);
        if (!dir.exists(newName)) {
            return newName;
        }
    }

    // 极端情况：使用时间戳
    return suffix.isEmpty()
        ? QStringLiteral("%1_%2").arg(baseName).arg(QDateTime::currentMSecsSinceEpoch())
        : QStringLiteral("%1_%2.%3").arg(baseName).arg(QDateTime::currentMSecsSinceEpoch()).arg(suffix);
}

QString FileUtils::getDefaultSavePath()
{
    QString documentsPath = QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation);
    QString savePath = QDir(documentsPath).filePath("EnhanceVision/output");
    ensureDirectory(savePath);
    return savePath;
}

void FileUtils::openInExplorer(const QString &filePath)
{
#ifdef Q_OS_WIN
    QProcess::startDetached("explorer", {"/select,", QDir::toNativeSeparators(filePath)});
#elif defined(Q_OS_MACOS)
    QProcess::startDetached("open", {"-R", filePath});
#else
    // Linux: 尝试用 xdg-open 打开所在目录
    QFileInfo fi(filePath);
    QProcess::startDetached("xdg-open", {fi.absolutePath()});
#endif
}

QString FileUtils::humanReadableSize(qint64 bytes)
{
    if (bytes < 1024) {
        return QStringLiteral("%1 B").arg(bytes);
    } else if (bytes < 1024LL * 1024) {
        return QStringLiteral("%1 KB").arg(bytes / 1024.0, 0, 'f', 1);
    } else if (bytes < 1024LL * 1024 * 1024) {
        return QStringLiteral("%1 MB").arg(bytes / (1024.0 * 1024.0), 0, 'f', 1);
    } else {
        return QStringLiteral("%1 GB").arg(bytes / (1024.0 * 1024.0 * 1024.0), 0, 'f', 2);
    }
}

} // namespace EnhanceVision
