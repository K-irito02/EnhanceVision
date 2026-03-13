/**
 * @file Application.cpp
 * @brief 应用程序封装类实现
 * @author Qt客户端开发工程师
 */

#include "EnhanceVision/app/Application.h"
#include "EnhanceVision/models/FileModel.h"
#include "EnhanceVision/models/SessionModel.h"
#include "EnhanceVision/models/ProcessingModel.h"
#include "EnhanceVision/controllers/SettingsController.h"
#include "EnhanceVision/controllers/FileController.h"
#include "EnhanceVision/controllers/SessionController.h"
#include "EnhanceVision/controllers/ProcessingController.h"
#include "EnhanceVision/providers/PreviewProvider.h"
#include "EnhanceVision/providers/ThumbnailProvider.h"
#include <QQmlEngine>
#include <QQmlContext>

namespace EnhanceVision {

Application::Application(QObject *parent)
    : QObject(parent)
    , m_mainWidget(new QQuickWidget)
    , m_fileModel(std::make_unique<FileModel>(this))
    , m_sessionModel(std::make_unique<SessionModel>(this))
    , m_processingModel(std::make_unique<ProcessingModel>(this))
    , m_fileController(std::make_unique<FileController>(this))
    , m_sessionController(std::make_unique<SessionController>(this))
    , m_processingController(std::make_unique<ProcessingController>(this))
{
    // 将 FileController 的 FileModel 替换为我们统一管理的实例
    m_fileController->setFileModel(m_fileModel.get());
}

Application::~Application()
{
    delete m_mainWidget;
    SettingsController::destroyInstance();
}

void Application::initialize()
{
    // 注册图像提供者
    QQmlEngine *engine = m_mainWidget->engine();
    engine->addImageProvider(QStringLiteral("preview"), new PreviewProvider());
    engine->addImageProvider(QStringLiteral("thumbnail"), new ThumbnailProvider());

    // 注册 QML 类型
    registerQmlTypes();

    // 设置 QML 引擎属性
    m_mainWidget->setResizeMode(QQuickWidget::SizeRootObjectToView);

    // 设置 QML 上下文属性
    setupQmlContext();

    // 加载 QML
    m_mainWidget->setSource(QUrl(QStringLiteral("qrc:/qt/qml/EnhanceVision/qml/main.qml")));

    // 检查 QML 加载错误
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
    // 注册数据模型类型
    qmlRegisterUncreatableType<FileModel>("EnhanceVision.Models", 1, 0, "FileModel",
                                           "FileModel should be accessed via context property");
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

    // 将数据模型设置为上下文属性，供 QML 访问
    context->setContextProperty("fileModel", m_fileModel.get());
    context->setContextProperty("sessionModel", m_sessionModel.get());
    context->setContextProperty("processingModel", m_processingModel.get());

    // 将控制器设置为上下文属性，供 QML 访问
    context->setContextProperty("fileController", m_fileController.get());
    context->setContextProperty("sessionController", m_sessionController.get());
    context->setContextProperty("processingController", m_processingController.get());
}

} // namespace EnhanceVision
