/**
 * @file ImageExportService.cpp
 * @brief 图像导出服务实现
 * @author Qt客户端开发工程师
 */

#include "EnhanceVision/services/ImageExportService.h"
#include <QDebug>
#include <QMetaObject>
#include <QQuickItemGrabResult>

namespace EnhanceVision {

ImageExportService* ImageExportService::s_instance = nullptr;

ImageExportService::ImageExportService(QObject* parent)
    : QObject(parent)
{
    s_instance = this;
}

ImageExportService* ImageExportService::instance()
{
    if (!s_instance) {
        s_instance = new ImageExportService();
    }
    return s_instance;
}

void ImageExportService::setQmlEngine(QObject* rootObject)
{
    m_rootObject = rootObject;
}

void ImageExportService::exportFromItem(QQuickItem* item, const QString& outputPath)
{
    if (!item) {
        qWarning() << "[ImageExportService] Item is null";
        emit exportCompleted("", false, outputPath, "Item is null");
        return;
    }

    qDebug() << "[ImageExportService] Exporting from item to:" << outputPath;

    auto result = item->grabToImage();
    if (!result) {
        qWarning() << "[ImageExportService] grabToImage failed";
        emit exportCompleted("", false, outputPath, "grabToImage failed");
        return;
    }

    connect(result.data(), &QQuickItemGrabResult::ready, this, [this, result, outputPath]() {
        QImage image = result->image();
        if (image.isNull()) {
            qWarning() << "[ImageExportService] Grabbed image is null";
            emit exportCompleted("", false, outputPath, "Grabbed image is null");
            return;
        }

        if (image.save(outputPath)) {
            qDebug() << "[ImageExportService] Image saved successfully:" << outputPath;
            emit exportCompleted("", true, outputPath, "");
        } else {
            qWarning() << "[ImageExportService] Failed to save image:" << outputPath;
            emit exportCompleted("", false, outputPath, "Failed to save image");
        }
    });
}

void ImageExportService::requestExport(
    const QString& exportId,
    const QString& imagePath,
    const QVariantMap& shaderParams,
    const QString& outputPath)
{
    qDebug() << "[ImageExportService] Requesting export:" << exportId
             << "image:" << imagePath
             << "output:" << outputPath;

    {
        QMutexLocker locker(&m_mutex);
        m_pendingExports[exportId] = true;
    }

    emit exportRequested(exportId, imagePath, shaderParams, outputPath);
}

void ImageExportService::reportExportResult(
    const QString& exportId,
    bool success,
    const QString& outputPath,
    const QString& error)
{
    qDebug() << "[ImageExportService] Export result:" << exportId
             << "success:" << success
             << "output:" << outputPath
             << "error:" << error;

    {
        QMutexLocker locker(&m_mutex);
        m_pendingExports.remove(exportId);
    }

    emit exportCompleted(exportId, success, outputPath, error);
}

} // namespace EnhanceVision
