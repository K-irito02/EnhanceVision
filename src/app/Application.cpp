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
#include "EnhanceVision/core/LifecycleSupervisor.h"
#include "EnhanceVision/utils/WindowHelper.h"
#include "EnhanceVision/utils/SubWindowHelper.h"
#include <QQmlEngine>
#include <QQmlContext>
#include <QCoreApplication>
#include <QApplication>
#include <QGuiApplication>
#include <QLibraryInfo>
#include <QQuickWindow>
#include <QWindow>
#include <QEvent>
#include <QElapsedTimer>
#include <QVector>
#include <QPointer>
#include <QTimer>
#include <algorithm>
#include <cmath>
#include <limits>
#include <cstdlib>

namespace EnhanceVision {

namespace {

class FrameTimeProbe final : public QObject
{
public:
    explicit FrameTimeProbe(QQuickWindow* window, QObject* parent = nullptr)
        : QObject(parent)
        , m_window(window)
    {
        if (m_window) {
            connect(m_window, &QQuickWindow::frameSwapped,
                    this, &FrameTimeProbe::onFrameSwapped);
        }

        m_timer.start();

        m_reportTimer = new QTimer(this);
        m_reportTimer->setInterval(10000);
        connect(m_reportTimer, &QTimer::timeout, this, [this]() {
            reportAndReset();
        });
        m_reportTimer->start();
    }

    ~FrameTimeProbe() override
    {
        reportAndReset(true);
    }

private:
    void onFrameSwapped()
    {
        const qint64 nowNs = m_timer.nsecsElapsed();
        if (m_lastNs > 0) {
            const double frameMs = static_cast<double>(nowNs - m_lastNs) / 1000000.0;
            if (frameMs > 0.0 && frameMs < std::numeric_limits<double>::infinity()) {
                m_samples.push_back(frameMs);
                if (frameMs > 16.7) {
                    m_over16Count++;
                }
                if (frameMs > 33.3) {
                    m_over33Count++;
                }
                if (frameMs > 50.0) {
                    m_over50Count++;
                }
                if (frameMs > 100.0) {
                    m_over100Count++;
                }
            }
        }
        m_lastNs = nowNs;
    }

private:
    static double percentileValue(const QVector<double>& values, double p)
    {
        if (values.isEmpty()) {
            return 0.0;
        }

        QVector<double> sorted = values;
        std::sort(sorted.begin(), sorted.end());

        const int sampleCount = static_cast<int>(sorted.size());
        int index = static_cast<int>(std::ceil((p / 100.0) * sampleCount)) - 1;
        index = std::max(0, std::min(index, sampleCount - 1));
        return sorted.at(index);
    }

    static double averageValue(const QVector<double>& values)
    {
        if (values.isEmpty()) {
            return 0.0;
        }

        double sum = 0.0;
        for (const double value : values) {
            sum += value;
        }
        return sum / static_cast<double>(values.size());
    }

    static double maxValue(const QVector<double>& values)
    {
        if (values.isEmpty()) {
            return 0.0;
        }

        return *std::max_element(values.begin(), values.end());
    }

    void reportAndReset(bool isFinalReport = false)
    {
        if (m_samples.isEmpty()) {
            return;
        }

        const double p50 = percentileValue(m_samples, 50.0);
        const double p95 = percentileValue(m_samples, 95.0);
        const double p99 = percentileValue(m_samples, 99.0);
        const double avg = averageValue(m_samples);
        const double max = maxValue(m_samples);

        qInfo().noquote() << QString("[Perf][Frame]%1 window:10s samples:%2 avg:%3ms p50:%4ms p95:%5ms p99:%6ms max:%7ms >16.7ms:%8 >33.3ms:%9 >50ms:%10 >100ms:%11")
                                 .arg(isFinalReport ? "[Final]" : "")
                                 .arg(m_samples.size())
                                 .arg(QString::number(avg, 'f', 2))
                                 .arg(QString::number(p50, 'f', 2))
                                 .arg(QString::number(p95, 'f', 2))
                                 .arg(QString::number(p99, 'f', 2))
                                 .arg(QString::number(max, 'f', 2))
                                 .arg(m_over16Count)
                                 .arg(m_over33Count)
                                 .arg(m_over50Count)
                                 .arg(m_over100Count);

        m_samples.clear();
        m_over16Count = 0;
        m_over33Count = 0;
        m_over50Count = 0;
        m_over100Count = 0;
    }

private:
    QPointer<QQuickWindow> m_window;
    QElapsedTimer m_timer;
    qint64 m_lastNs = 0;
    QTimer* m_reportTimer = nullptr;
    QVector<double> m_samples;
    int m_over16Count = 0;
    int m_over33Count = 0;
    int m_over50Count = 0;
    int m_over100Count = 0;
};

} // namespace

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
    qInfo() << "[Application] Destructor called";

    SettingsController::instance()->markAppExiting();

    m_sessionController->saveSessions();
    delete m_mainWidget;
    SettingsController::destroyInstance();

    qInfo() << "[Application] Destructor completed";
}

void Application::initialize()
{
    m_mainWidget->setWindowFlags(Qt::FramelessWindowHint);

    QQmlEngine *engine = m_mainWidget->engine();
    engine->addImageProvider(QStringLiteral("preview"), new PreviewProvider());

    auto* thumbnailProvider = new ThumbnailProvider();
    engine->addImageProvider(QStringLiteral("thumbnail"), thumbnailProvider);

    registerQmlTypes();

    m_mainWidget->setResizeMode(QQuickWidget::SizeRootObjectToView);

    setupQmlContext(thumbnailProvider);

    setupTranslator();

    WindowHelper::instance()->setWindow(m_mainWidget);

    setupLifecycleGuard();

    if (SettingsController::instance()->checkAndHandleCrashRecovery()) {
        QTimer::singleShot(500, this, [this]() {
            emit crashRecoveryNeeded();
        });
    }

    SettingsController::instance()->markAppRunning();

    m_sessionController->loadSessions();
    m_sessionController->restoreThumbnails();
    
    // 延迟检查中断任务，确保 UI 完全加载后再开始自动重处理
    QTimer::singleShot(1000, this, [this]() {
        m_sessionController->checkAndAutoRetryAllInterruptedTasks();
    });

    m_mainWidget->setSource(QUrl(QStringLiteral("qrc:/qt/qml/EnhanceVision/qml/main.qml")));

    if (auto* quickWindow = m_mainWidget->quickWindow()) {
        new FrameTimeProbe(quickWindow, m_mainWidget);
        qInfo() << "[Perf][Frame] probe attached";
    } else {
        qWarning() << "[Perf][Frame] probe attach failed: quickWindow is null";
    }

    if (m_mainWidget->status() == QQuickWidget::Error) {
        const auto errors = m_mainWidget->errors();
        for (const auto &error : errors) {
            qCritical() << "QML Error:" << error.toString();
        }
        if (m_lifecycleSupervisor) {
            m_lifecycleSupervisor->requestShutdown(
                ExitReason::QmlLoadError,
                QStringLiteral("QQuickWidget load failed"));
        }
    }
}

void Application::show()
{
    m_mainWidget->resize(1280, 720);
    m_mainWidget->show();
    m_mainWindowEverShown = true;
}

void Application::setupLifecycleGuard()
{
    m_lifecycleSupervisor = LifecycleSupervisor::instance();
    m_lifecycleSupervisor->setProcessingController(m_processingController.get());
    m_lifecycleSupervisor->setupWindowWatchdog(m_mainWidget);
    m_lifecycleSupervisor->setupProcessWatchdog();

    if (auto* app = qobject_cast<QApplication*>(QCoreApplication::instance())) {
        app->setQuitOnLastWindowClosed(true);

        connect(app, &QApplication::lastWindowClosed, this, [this]() {
            qWarning() << "[Application] lastWindowClosed emitted";
            if (m_lifecycleSupervisor) {
                m_lifecycleSupervisor->requestShutdown(ExitReason::MainWindowClosed);
            }
        });
    }

    qInfo() << "[Application] Lifecycle guard setup completed";
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

void Application::setupQmlContext(ThumbnailProvider* thumbnailProvider)
{
    QQmlContext *context = m_mainWidget->rootContext();

    // 设置控制器之间的连接
    m_processingController->setFileController(m_fileController.get());
    m_processingController->setMessageModel(m_messageModel.get());
    m_processingController->setSessionController(m_sessionController.get());
    m_sessionController->setProcessingController(m_processingController.get());
    m_messageModel->setProcessingController(m_processingController.get());
    
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
    
    // 将 ThumbnailProvider 设置为上下文属性，供 QML 访问
    context->setContextProperty("thumbnailProvider", thumbnailProvider);
    
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
