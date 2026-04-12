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
#include "EnhanceVision/controllers/TaskRecoveryController.h"
#include "EnhanceVision/controllers/UIStateController.h"
#include "EnhanceVision/services/ImageExportService.h"
#include "EnhanceVision/services/AutoSaveService.h"
#include "EnhanceVision/providers/PreviewProvider.h"
#include "EnhanceVision/providers/ThumbnailProvider.h"
#include "EnhanceVision/core/ThumbnailDatabase.h"
#include "EnhanceVision/core/LifecycleSupervisor.h"
#include "EnhanceVision/core/TaskTimeEstimator.h"
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
#include <QWidget>
#include <QDir>
#include <QElapsedTimer>
#include <QVector>
#include <QPointer>
#include <QTimer>
#include <QScreen>
#include <algorithm>
#include <cmath>
#include <limits>
#include <cstdlib>

namespace EnhanceVision {

namespace {

constexpr auto kMainWindowLayoutKey = "main";
constexpr int kDefaultMainWindowWidth = 1280;
constexpr int kDefaultMainWindowHeight = 720;
constexpr int kMinVisibleInset = 80;

bool isEnglishLanguage(const QString& language)
{
    const QString lower = language.toLower();
    return lower.startsWith(QStringLiteral("en"));
}

QString languageBase(const QString& language)
{
    const int separatorIndex = language.indexOf(QLatin1Char('_'));
    if (separatorIndex > 0) {
        return language.left(separatorIndex);
    }
    return language;
}

QStringList uniqueStringList(const QStringList& list)
{
    QStringList result;
    result.reserve(list.size());
    for (const QString& value : list) {
        if (value.isEmpty() || result.contains(value)) {
            continue;
        }
        result.append(value);
    }
    return result;
}

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
    , m_taskRecoveryController(std::make_unique<TaskRecoveryController>(this))
    , m_imageExportService(ImageExportService::instance())
{
    m_fileController->setFileModel(m_fileModel.get());

    m_mainWindowLayoutSaveTimer = new QTimer(this);
    m_mainWindowLayoutSaveTimer->setSingleShot(true);
    m_mainWindowLayoutSaveTimer->setInterval(250);
    connect(m_mainWindowLayoutSaveTimer, &QTimer::timeout,
            this, &Application::saveMainWindowLayoutNow);
}

Application::~Application()
{
    saveMainWindowLayoutNow();
    if (m_taskRecoveryController) {
        m_taskRecoveryController->persistSnapshotNow();
    }
    m_sessionController->saveSessions();
    ThumbnailDatabase::destroyInstance();
    delete m_mainWidget;
    SettingsController::destroyInstance();
}

void Application::initialize()
{
    m_mainWidget->setWindowFlags(Qt::Window | Qt::FramelessWindowHint);
    m_mainWidget->installEventFilter(this);

    QQmlEngine *engine = m_mainWidget->engine();
    engine->addImageProvider(QStringLiteral("preview"), new PreviewProvider());

    QString dataDir = SettingsController::instance()->effectiveDataPath();

    auto* db = ThumbnailDatabase::instance();
    if (!db->initialize(dataDir)) {
        qWarning() << "[Application] ThumbnailDatabase initialization failed, using memory-only mode";
    }

    auto* thumbnailProvider = new ThumbnailProvider();
    engine->addImageProvider(QStringLiteral("thumbnail"), thumbnailProvider);
    thumbnailProvider->initializePersistence();

    registerQmlTypes();

    m_mainWidget->setResizeMode(QQuickWidget::SizeRootObjectToView);

    setupQmlContext(thumbnailProvider);

    setupTranslator();

    WindowHelper::instance()->setWindow(m_mainWidget);

    setupLifecycleGuard();

    SettingsController::instance()->checkAndHandleCrashRecovery();

    SettingsController::instance()->markAppRunning();

    AutoSaveService::instance()->initialize();

    // 初始化 UIStateController
    UIStateController::instance();

    m_sessionController->loadSessions();
    m_sessionController->restoreThumbnails();
    m_taskRecoveryController->initializeStartupRecovery();

    m_mainWidget->setSource(QUrl(QStringLiteral("qrc:/qt/qml/EnhanceVision/qml/main.qml")));

    if (auto* quickWindow = m_mainWidget->quickWindow()) {
        new FrameTimeProbe(quickWindow, m_mainWidget);
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
    applyMainWindowLayout();
    m_mainWidget->raise();
    m_mainWidget->activateWindow();
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
            if (m_lifecycleSupervisor) {
                m_lifecycleSupervisor->requestShutdown(ExitReason::MainWindowClosed);
            }
        });
    }
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

    // 注册 UIStateController 为单例
    qmlRegisterSingletonType<UIStateController>("EnhanceVision.Controllers", 1, 0, "UIStateController",
        [](QQmlEngine*, QJSEngine*) -> QObject* {
            return UIStateController::instance();
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
    m_processingController->setProcessingTimeManager(m_sessionController->processingTimeManager());
    m_taskRecoveryController->setSessionController(m_sessionController.get());
    m_taskRecoveryController->setProcessingController(m_processingController.get());
    m_sessionController->setProcessingController(m_processingController.get());
    m_sessionController->setTaskRecoveryController(m_taskRecoveryController.get());
    m_messageModel->setProcessingController(m_processingController.get());
    
    // 设置 SessionController 引用到 SettingsController（用于缓存清理）
    SettingsController::instance()->setSessionController(m_sessionController.get());
    
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
    context->setContextProperty("taskRecoveryController", m_taskRecoveryController.get());
    
    // 将 ThumbnailProvider 设置为上下文属性，供 QML 访问
    context->setContextProperty("thumbnailProvider", thumbnailProvider);
    
    // 将 ImageExportService 设置为上下文属性，供 QML 访问
    context->setContextProperty("imageExportService", m_imageExportService);
    
    // 初始化并注册 TaskTimeEstimator
    TaskTimeEstimator::instance()->initialize();
    context->setContextProperty("taskTimeEstimator", TaskTimeEstimator::instance());
    
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
    } else {
        qWarning() << "[Application] Failed to load app translator:" << qmFile;
    }

    m_qtTranslator.reset(new QTranslator);
    if (!isEnglishLanguage(language)) {
        const QString base = languageBase(language);
        const QStringList candidates = uniqueStringList({
            QStringLiteral("qtbase_%1").arg(language),
            QStringLiteral("qt_%1").arg(language),
            QStringLiteral("qtbase_%1").arg(base),
            QStringLiteral("qt_%1").arg(base)
        });
        const QStringList searchPaths = uniqueStringList({
            QDir::cleanPath(QCoreApplication::applicationDirPath() + QStringLiteral("/translations")),
            QLibraryInfo::path(QLibraryInfo::TranslationsPath)
        });

        bool loaded = false;
        for (const QString& path : searchPaths) {
            for (const QString& candidate : candidates) {
                if (m_qtTranslator->load(candidate, path)) {
                    loaded = true;
                    break;
                }
            }
            if (loaded) {
                break;
            }
        }

        if (loaded) {
            QCoreApplication::installTranslator(m_qtTranslator.data());
        } else {
            qWarning() << "[Application] Failed to load Qt translator for language:"
                       << language << "candidates:" << candidates << "paths:" << searchPaths;
        }
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

bool Application::eventFilter(QObject* watched, QEvent* event)
{
    if (watched == m_mainWidget && m_mainWidget) {
        switch (event->type()) {
        case QEvent::Move:
        case QEvent::Resize:
        case QEvent::WindowStateChange:
            scheduleMainWindowLayoutSave();
            break;
        case QEvent::Close:
            saveMainWindowLayoutNow();
            break;
        default:
            break;
        }
    }

    return QObject::eventFilter(watched, event);
}

QRect Application::defaultMainWindowGeometry() const
{
    const QScreen* screen = m_mainWidget && m_mainWidget->screen()
        ? m_mainWidget->screen()
        : QGuiApplication::primaryScreen();
    const QRect available = screen ? screen->availableGeometry() : QRect(0, 0, kDefaultMainWindowWidth, kDefaultMainWindowHeight);

    const int width = std::min(kDefaultMainWindowWidth, available.width());
    const int height = std::min(kDefaultMainWindowHeight, available.height());
    const int x = available.x() + std::max(0, (available.width() - width) / 2);
    const int y = available.y() + std::max(0, (available.height() - height) / 2);
    return QRect(x, y, width, height);
}

QRect Application::sanitizeMainWindowGeometry(const QRect& geometry) const
{
    if (!geometry.isValid() || geometry.width() <= 0 || geometry.height() <= 0) {
        return defaultMainWindowGeometry();
    }

    const QList<QScreen*> screens = QGuiApplication::screens();
    if (screens.isEmpty()) {
        return geometry;
    }

    QScreen* targetScreen = nullptr;
    for (QScreen* screen : screens) {
        if (screen->availableGeometry().intersects(geometry)) {
            targetScreen = screen;
            break;
        }
    }

    if (!targetScreen) {
        targetScreen = m_mainWidget && m_mainWidget->screen()
            ? m_mainWidget->screen()
            : QGuiApplication::primaryScreen();
    }

    if (!targetScreen) {
        return geometry;
    }

    const QRect available = targetScreen->availableGeometry();
    const int width = std::min(geometry.width(), available.width());
    const int height = std::min(geometry.height(), available.height());
    const int insetX = std::min(kMinVisibleInset, std::max(0, width / 2));
    const int insetY = std::min(kMinVisibleInset, std::max(0, height / 2));
    const int minX = available.left() - width + insetX;
    const int maxX = available.right() - insetX + 1;
    const int minY = available.top();
    const int maxY = available.bottom() - insetY + 1;
    const int x = std::clamp(geometry.x(), minX, maxX);
    const int y = std::clamp(geometry.y(), minY, maxY);
    return QRect(x, y, width, height);
}

void Application::applyMainWindowLayout()
{
    const auto* uiStateController = UIStateController::instance();
    const QString layoutKey = QString::fromLatin1(kMainWindowLayoutKey);
    QRect targetGeometry = defaultMainWindowGeometry();
    bool openMaximized = false;

    if (uiStateController->hasWindowLayout(layoutKey)) {
        const auto layout = uiStateController->windowLayout(layoutKey);
        if (layout.isValid()) {
            targetGeometry = sanitizeMainWindowGeometry(layout.normalGeometry);
            openMaximized = layout.maximized;
        }
    }

    m_mainWidget->setGeometry(targetGeometry);
    if (openMaximized) {
        m_mainWidget->showMaximized();
    } else {
        m_mainWidget->show();
    }
}

void Application::scheduleMainWindowLayoutSave()
{
    if (!m_mainWidget || !m_mainWindowEverShown || m_mainWidget->isMinimized()) {
        return;
    }

    if (m_mainWindowLayoutSaveTimer) {
        m_mainWindowLayoutSaveTimer->start();
    }
}

void Application::saveMainWindowLayoutNow()
{
    if (!m_mainWidget || !m_mainWindowEverShown) {
        return;
    }

    const bool maximized = m_mainWidget->isMaximized();
    const QRect normalGeometry = maximized ? m_mainWidget->normalGeometry() : m_mainWidget->geometry();
    if (!normalGeometry.isValid() || normalGeometry.width() <= 0 || normalGeometry.height() <= 0) {
        return;
    }

    UIStateController::instance()->setWindowLayout(
        QString::fromLatin1(kMainWindowLayoutKey),
        normalGeometry,
        maximized);
}

} // namespace EnhanceVision
