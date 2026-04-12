/**
 * @file Application.h
 * @brief 应用程序封装类
 * @author Qt客户端开发工程师
 */

#ifndef ENHANCEVISION_APPLICATION_H
#define ENHANCEVISION_APPLICATION_H

#include <QObject>
#include <QPointer>
#include <QQmlApplicationEngine>
#include <QTranslator>
#include <QSharedPointer>
#include <QRect>
#include <QString>
#include <QTimer>
#include <memory>

class QQuickWindow;

namespace EnhanceVision {

class FileModel;
class MessageModel;
class SessionModel;
class ProcessingModel;
class SettingsController;
class FileController;
class SessionController;
class ProcessingController;
class TaskRecoveryController;
class ImageExportService;
class ThumbnailProvider;
class LifecycleSupervisor;

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
    void registerQmlTypes();
    void setupQmlContext(ThumbnailProvider* thumbnailProvider);
    void setupTranslator();
    bool switchTranslator(const QString& language);

    void setupLifecycleGuard();
    bool eventFilter(QObject* watched, QEvent* event) override;
    QRect defaultMainWindowGeometry() const;
    QRect sanitizeMainWindowGeometry(const QRect& geometry) const;
    void applyMainWindowLayout();
    void scheduleMainWindowLayoutSave();
    void saveMainWindowLayoutNow();

    QQmlApplicationEngine* m_qmlEngine;
    QPointer<QQuickWindow> m_mainWindow;
    QPointer<QObject> m_rootObject;
    std::unique_ptr<FileModel> m_fileModel;
    std::unique_ptr<MessageModel> m_messageModel;
    std::unique_ptr<SessionModel> m_sessionModel;
    std::unique_ptr<ProcessingModel> m_processingModel;
    std::unique_ptr<FileController> m_fileController;
    std::unique_ptr<SessionController> m_sessionController;
    std::unique_ptr<ProcessingController> m_processingController;
    std::unique_ptr<TaskRecoveryController> m_taskRecoveryController;
    ImageExportService *m_imageExportService;
    QSharedPointer<QTranslator> m_translator;
    QSharedPointer<QTranslator> m_qtTranslator;
    QTimer* m_mainWindowLayoutSaveTimer = nullptr;

    LifecycleSupervisor* m_lifecycleSupervisor = nullptr;
    bool m_mainWindowEverShown = false;

private slots:
    void onLanguageChanged();

signals:
    void crashRecoveryNeeded();
};

} // namespace EnhanceVision

#endif // ENHANCEVISION_APPLICATION_H
