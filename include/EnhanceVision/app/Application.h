/**
 * @file Application.h
 * @brief 应用程序封装类
 * @author Qt客户端开发工程师
 */

#ifndef ENHANCEVISION_APPLICATION_H
#define ENHANCEVISION_APPLICATION_H

#include <QObject>
#include <QQuickWidget>
#include <memory>

namespace EnhanceVision {

class FileModel;
class MessageModel;
class SessionModel;
class ProcessingModel;
class SettingsController;
class FileController;
class SessionController;
class ProcessingController;
class ImageExportService;

class Application : public QObject
{
    Q_OBJECT

public:
    explicit Application(QObject *parent = nullptr);
    ~Application() override;

    /**
     * @brief 初始化应用程序
     */
    void initialize();

    /**
     * @brief 显示主窗口
     */
    void show();

private:
    /**
     * @brief 注册 QML 类型
     */
    void registerQmlTypes();

    /**
     * @brief 设置 QML 上下文属性
     */
    void setupQmlContext();

    QQuickWidget *m_mainWidget;
    std::unique_ptr<FileModel> m_fileModel;
    std::unique_ptr<MessageModel> m_messageModel;
    std::unique_ptr<SessionModel> m_sessionModel;
    std::unique_ptr<ProcessingModel> m_processingModel;
    std::unique_ptr<FileController> m_fileController;
    std::unique_ptr<SessionController> m_sessionController;
    std::unique_ptr<ProcessingController> m_processingController;
    ImageExportService *m_imageExportService;
};

} // namespace EnhanceVision

#endif // ENHANCEVISION_APPLICATION_H
