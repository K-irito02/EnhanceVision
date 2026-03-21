/**
 * @file ImageExportService.cpp
 * @brief 图像导出服务实现
 * @author Qt客户端开发工程师
 */

#include "EnhanceVision/services/ImageExportService.h"
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
        emit exportCompleted("", false, outputPath, "Item is null");
        return;
    }

    auto result = item->grabToImage();
    if (!result) {
        emit exportCompleted("", false, outputPath, "grabToImage failed");
        return;
    }

    connect(result.data(), &QQuickItemGrabResult::ready, this, [this, result, outputPath]() {
        QImage image = result->image();
        if (image.isNull()) {
            emit exportCompleted("", false, outputPath, "Grabbed image is null");
            return;
        }

        if (image.save(outputPath)) {
            emit exportCompleted("", true, outputPath, "");
        } else {
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
    {
        QMutexLocker locker(&m_mutex);
        m_pendingExports.remove(exportId);
    }

    emit exportCompleted(exportId, success, outputPath, error);
}

} // namespace EnhanceVision
