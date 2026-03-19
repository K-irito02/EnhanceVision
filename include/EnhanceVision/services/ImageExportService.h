/**
 * @file ImageExportService.h
 * @brief 图像导出服务 - 使用 GPU 渲染导出图像
 * @author Qt客户端开发工程师
 */

#ifndef ENHANCEVISION_IMAGEEXPORTSERVICE_H
#define ENHANCEVISION_IMAGEEXPORTSERVICE_H

#include <QObject>
#include <QQuickItem>
#include <QVariantMap>
#include <QHash>
#include <QMutex>

namespace EnhanceVision {

/**
 * @brief 图像导出服务
 * 
 * 使用 GPU Shader 渲染并通过 grabToImage 导出图像，
 * 确保预览效果与导出效果完全一致。
 */
class ImageExportService : public QObject
{
    Q_OBJECT

public:
    static ImageExportService* instance();

    void setQmlEngine(QObject* rootObject);

    Q_INVOKABLE void exportFromItem(QQuickItem* item, const QString& outputPath);
    
    Q_INVOKABLE void requestExport(
        const QString& exportId,
        const QString& imagePath,
        const QVariantMap& shaderParams,
        const QString& outputPath
    );

    Q_INVOKABLE void reportExportResult(
        const QString& exportId,
        bool success,
        const QString& outputPath,
        const QString& error
    );

signals:
    void exportRequested(
        const QString& exportId,
        const QString& imagePath,
        const QVariantMap& shaderParams,
        const QString& outputPath
    );
    
    void exportCompleted(const QString& exportId, bool success, const QString& outputPath, const QString& error);
    void exportProgress(const QString& exportId, int progress);

private:
    explicit ImageExportService(QObject* parent = nullptr);
    ~ImageExportService() override = default;

    static ImageExportService* s_instance;
    QObject* m_rootObject = nullptr;
    QMutex m_mutex;
    QHash<QString, bool> m_pendingExports;
};

} // namespace EnhanceVision

#endif // ENHANCEVISION_IMAGEEXPORTSERVICE_H
