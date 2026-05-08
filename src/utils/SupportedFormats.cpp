#include "EnhanceVision/utils/SupportedFormats.h"
#include <QFileInfo>

namespace EnhanceVision {

const QStringList& SupportedFormats::imageExtensions()
{
    static const QStringList exts = {
        QStringLiteral("jpg"),
        QStringLiteral("jpeg"),
        QStringLiteral("png"),
        QStringLiteral("bmp"),
        QStringLiteral("webp"),
        QStringLiteral("tiff"),
        QStringLiteral("tif"),
        QStringLiteral("gif"),
        QStringLiteral("ico"),
        QStringLiteral("svg")
    };
    return exts;
}

const QStringList& SupportedFormats::videoExtensions()
{
    static const QStringList exts = {
        QStringLiteral("mp4"),
        QStringLiteral("avi"),
        QStringLiteral("mkv"),
        QStringLiteral("mov"),
        QStringLiteral("flv"),
        QStringLiteral("wmv"),
        QStringLiteral("webm"),
        QStringLiteral("m4v"),
        QStringLiteral("mpg"),
        QStringLiteral("mpeg"),
        QStringLiteral("ts"),
        QStringLiteral("mts"),
        QStringLiteral("m2ts"),
        QStringLiteral("3gp")
    };
    return exts;
}

const QStringList& SupportedFormats::allExtensions()
{
    static const QStringList exts = []() {
        QStringList all;
        all << imageExtensions() << videoExtensions();
        return all;
    }();
    return exts;
}

QString SupportedFormats::imageFileDialogFilter()
{
    QStringList patterns;
    for (const auto& fmt : imageExtensions()) {
        patterns << QString("*.%1").arg(fmt);
    }
    return QObject::tr("图片文件 (%1)").arg(patterns.join(" "));
}

QString SupportedFormats::videoFileDialogFilter()
{
    QStringList patterns;
    for (const auto& fmt : videoExtensions()) {
        patterns << QString("*.%1").arg(fmt);
    }
    return QObject::tr("视频文件 (%1)").arg(patterns.join(" "));
}

QString SupportedFormats::allSupportedFileDialogFilter()
{
    QStringList patterns;
    for (const auto& fmt : allExtensions()) {
        patterns << QString("*.%1").arg(fmt);
    }
    return QObject::tr("所有支持的文件 (%1)").arg(patterns.join(" "));
}

QString SupportedFormats::allFilesDialogFilter()
{
    return QObject::tr("所有文件 (*.*)");
}

bool SupportedFormats::isImageFile(const QString& filePath)
{
    QFileInfo fileInfo(filePath);
    return imageExtensions().contains(fileInfo.suffix().toLower());
}

bool SupportedFormats::isVideoFile(const QString& filePath)
{
    QFileInfo fileInfo(filePath);
    return videoExtensions().contains(fileInfo.suffix().toLower());
}

bool SupportedFormats::isFormatSupported(const QString& filePath)
{
    return isImageFile(filePath) || isVideoFile(filePath);
}

} // namespace EnhanceVision
