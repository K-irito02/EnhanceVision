#ifndef ENHANCEVISION_SUPPORTEDFORMATS_H
#define ENHANCEVISION_SUPPORTEDFORMATS_H

#include <QString>
#include <QStringList>

namespace EnhanceVision {

class SupportedFormats
{
public:
    static const QStringList& imageExtensions();
    static const QStringList& videoExtensions();
    static const QStringList& allExtensions();

    static QString imageFileDialogFilter();
    static QString videoFileDialogFilter();
    static QString allSupportedFileDialogFilter();
    static QString allFilesDialogFilter();

    static bool isImageFile(const QString& filePath);
    static bool isVideoFile(const QString& filePath);
    static bool isFormatSupported(const QString& filePath);
};

} // namespace EnhanceVision

#endif // ENHANCEVISION_SUPPORTEDFORMATS_H
