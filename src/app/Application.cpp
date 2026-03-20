/**
 * @file Application.cpp
 * @brief 应用程序封装类实现
 * @author Qt客户端开发工程师
 */

#include "EnhanceVision/app/Application.h"
#include "EnhanceVision/models/FileModel.h"
#include "EnhanceVision/models/MessageModel.h"
#include "EnhanceVision/models/SessionModel.h"
#include "EnhanceVision/models/ProcessingModel.h"
#include "EnhanceVision/controllers/SettingsController.h"
#include "EnhanceVision/controllers/FileController.h"
#include "EnhanceVision/controllers/SessionController.h"
#include "EnhanceVision/controllers/ProcessingController.h"
#include "EnhanceVision/services/ImageExportService.h"
#include "EnhanceVision/providers/PreviewProvider.h"
#include "EnhanceVision/providers/ThumbnailProvider.h"
#include "EnhanceVision/utils/WindowHelper.h"
#include "EnhanceVision/utils/SubWindowHelper.h"
#include <QQmlEngine>
#include <QQmlContext>
#include <QCoreApplication>
#include <QLibraryInfo>

namespace EnhanceVision {

Application::Application(QObject *parent)
    : QObject(parent)
    , m_mainWidget(new QQuickWidget)
    , m_fileModel(std::make_unique<FileModel>(this))
    , m_messageModel(std::make_unique<MessageModel>(this))
    , m_sessionModel(std::make_unique<SessionModel>(this))
    , m_processingModel(std::make_unique<ProcessingModel>(this))
    , m_fileController(std::make_unique<FileController>(this))
    , m_sessionController(std::make_unique<SessionController>(this))
    , m_processingController(std::make_unique<ProcessingController>(this))
    , m_imageExportService(ImageExportService::instance())
{
    m_fileController->setFileModel(m_fileModel.get());
}

Application::~Application()
{
    m_sessionController->saveSessions();
    delete m_mainWidget;
    SettingsController::destroyInstance();
}

void Application::initialize()
{
    m_mainWidget->setWindowFlags(Qt::FramelessWindowHint);
    
    QQmlEngine *engine = m_mainWidget->engine();
    engine->addImageProvider(QStringLiteral("preview"), new PreviewProvider());
    engine->addImageProvider(QStringLiteral("thumbnail"), new ThumbnailProvider());

    registerQmlTypes();

    m_mainWidget->setResizeMode(QQuickWidget::SizeRootObjectToView);

    setupQmlContext();

    setupTranslator();

    WindowHelper::instance()->setWindow(m_mainWidget);

    m_sessionController->loadSessions();

    m_mainWidget->setSource(QUrl(QStringLiteral("qrc:/qt/qml/EnhanceVision/qml/main.qml")));

    if (m_mainWidget->status() == QQuickWidget::Error) {
        const auto errors = m_mainWidget->errors();
        for (const auto &error : errors) {
            qCritical() << "QML Error:" << error.toString();
        }
    }
}

void Application::show()
{
    m_mainWidget->resize(1280, 720);
    m_mainWidget->show();
}

void Application::registerQmlTypes()
{
    // 注册 WindowHelper 为单例
    qmlRegisterSingletonType<WindowHelper>("EnhanceVision.Utils", 1, 0, "WindowHelper",
        [](QQmlEngine*, QJSEngine*) -> QObject* {
            return WindowHelper::instance();
        });

    // 注册 SubWindowHelper 为可实例化类型（每个子窗口创建一个实例）
    qmlRegisterType<SubWindowHelper>("EnhanceVision.Utils", 1, 0, "SubWindowHelper");

    // 注册数据模型类型
    qmlRegisterUncreatableType<FileModel>("EnhanceVision.Models", 1, 0, "FileModel",
                                           "FileModel should be accessed via context property");
    qmlRegisterUncreatableType<MessageModel>("EnhanceVision.Models", 1, 0, "MessageModel",
                                              "MessageModel should be accessed via context property");
    qmlRegisterUncreatableType<SessionModel>("EnhanceVision.Models", 1, 0, "SessionModel",
                                              "SessionModel should be accessed via context property");
    qmlRegisterUncreatableType<ProcessingModel>("EnhanceVision.Models", 1, 0, "ProcessingModel",
                                                 "ProcessingModel should be accessed via context property");

    // 注册 SettingsController 为单例
    qmlRegisterSingletonType<SettingsController>("EnhanceVision.Controllers", 1, 0, "SettingsController",
        [](QQmlEngine*, QJSEngine*) -> QObject* {
            return SettingsController::instance();
        });

    // 注册其他控制器类型
    qmlRegisterUncreatableType<FileController>("EnhanceVision.Controllers", 1, 0, "FileController",
                                                 "FileController should be accessed via context property");
    qmlRegisterUncreatableType<SessionController>("EnhanceVision.Controllers", 1, 0, "SessionController",
                                                    "SessionController should be accessed via context property");
    qmlRegisterUncreatableType<ProcessingController>("EnhanceVision.Controllers", 1, 0, "ProcessingController",
                                                       "ProcessingController should be accessed via context property");
}

void Application::setupQmlContext()
{
    QQmlContext *context = m_mainWidget->rootContext();

    // 设置控制器之间的连接
    m_fileController->setFileModel(m_fileModel.get());
    m_processingController->setFileController(m_fileController.get());
    m_processingController->setMessageModel(m_messageModel.get());
    m_processingController->setSessionController(m_sessionController.get());
    
    // 连接 SessionController 和 MessageModel（用于会话切换时同步消息）
    m_sessionController->setMessageModel(m_messageModel.get());

    // 将数据模型设置为上下文属性，供 QML 访问
    context->setContextProperty("fileModel", m_fileModel.get());
    context->setContextProperty("messageModel", m_messageModel.get());
    context->setContextProperty("sessionModel", m_sessionModel.get());
    context->setContextProperty("processingModel", m_processingModel.get());

    // 将控制器设置为上下文属性，供 QML 访问
    context->setContextProperty("fileController", m_fileController.get());
    context->setContextProperty("sessionController", m_sessionController.get());
    context->setContextProperty("processingController", m_processingController.get());
    
    // 将 ImageExportService 设置为上下文属性，供 QML 访问
    context->setContextProperty("imageExportService", m_imageExportService);
    
    // 设置 ImageExportService 的 QML 根对象引用
    m_imageExportService->setQmlEngine(m_mainWidget->rootObject());
    
    // 连接 ProcessingController 和 ImageExportService
    connect(m_processingController.get(), &ProcessingController::requestShaderExport,
            m_imageExportService, &ImageExportService::requestExport);
    connect(m_imageExportService, &ImageExportService::exportCompleted,
            m_processingController.get(), &ProcessingController::onShaderExportCompleted);
}

void Application::setupTranslator()
{
    QString language = SettingsController::instance()->language();
    switchTranslator(language);

    connect(SettingsController::instance(), &SettingsController::languageChanged,
            this, &Application::onLanguageChanged);
}

bool Application::switchTranslator(const QString& language)
{
    if (m_translator) {
        QCoreApplication::removeTranslator(m_translator.data());
    }
    if (m_qtTranslator) {
        QCoreApplication::removeTranslator(m_qtTranslator.data());
    }

    m_translator.reset(new QTranslator);
    QString qmFile = QString(":/i18n/app_%1.qm").arg(language);
    if (m_translator->load(qmFile)) {
        QCoreApplication::installTranslator(m_translator.data());
    }

    m_qtTranslator.reset(new QTranslator);
    QString qtQmFile = QString("qtbase_%1").arg(language);
    QString qtTranslationsPath = QLibraryInfo::path(QLibraryInfo::TranslationsPath);
    if (m_qtTranslator->load(qtQmFile, qtTranslationsPath)) {
        QCoreApplication::installTranslator(m_qtTranslator.data());
    }

    return true;
}

void Application::onLanguageChanged()
{
    QString language = SettingsController::instance()->language();
    switchTranslator(language);

    if (m_mainWidget && m_mainWidget->engine()) {
        m_mainWidget->engine()->retranslate();
    }
}

} // namespace EnhanceVision
