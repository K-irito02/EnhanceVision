#include "EnhanceVision/core/LifecycleSupervisor.h"
#include "EnhanceVision/controllers/ProcessingController.h"
#include <QWidget>
#include <QThreadPool>
#include <QCoreApplication>
#include <QElapsedTimer>
#include <QEventLoop>
#include <QDebug>

#ifdef Q_OS_WIN
#include <windows.h>
#include <processthreadsapi.h>
#endif

namespace EnhanceVision {

LifecycleSupervisor* LifecycleSupervisor::s_instance = nullptr;

LifecycleSupervisor* LifecycleSupervisor::instance()
{
    if (!s_instance) {
        s_instance = new LifecycleSupervisor();
    }
    return s_instance;
}

LifecycleSupervisor::LifecycleSupervisor(QObject* parent)
    : QObject(parent)
{
    qInfo() << "[LifecycleSupervisor] Created";
}

LifecycleSupervisor::~LifecycleSupervisor()
{
    qInfo() << "[LifecycleSupervisor] Destroying, final state:" << stateToString(m_state.load());
    s_instance = nullptr;
}

void LifecycleSupervisor::setProcessingController(ProcessingController* controller)
{
    m_processingController = controller;
}

void LifecycleSupervisor::setThreadPool(QThreadPool* pool)
{
    m_threadPool = pool;
}

void LifecycleSupervisor::setupWindowWatchdog(QWidget* mainWindow)
{
    if (!mainWindow) {
        qWarning() << "[LifecycleSupervisor] Cannot setup window watchdog: mainWindow is null";
        return;
    }

    m_mainWindow = mainWindow;
    m_windowEverShown = false;

    connect(mainWindow, &QWidget::destroyed, this, [this]() {
        qWarning() << "[LifecycleSupervisor] Main window destroyed";
        m_mainWindow = nullptr;
        if (!isShuttingDown()) {
            requestShutdown(ExitReason::MainWindowDestroyed);
        }
    });

    m_windowWatchdog = new QTimer(this);
    m_windowWatchdog->setInterval(300);
    connect(m_windowWatchdog, &QTimer::timeout, this, &LifecycleSupervisor::onWindowWatchdogTimeout);
    m_windowWatchdog->start();
    qInfo() << "[LifecycleSupervisor] Window watchdog started (interval: 300ms)";
}

void LifecycleSupervisor::setupProcessWatchdog()
{
    m_processWatchdog = new QTimer(this);
    m_processWatchdog->setInterval(1000);
    connect(m_processWatchdog, &QTimer::timeout, this, &LifecycleSupervisor::onProcessWatchdogTimeout);
    m_processWatchdog->start();
    qInfo() << "[LifecycleSupervisor] Process watchdog started (interval: 1000ms)";
}

void LifecycleSupervisor::requestShutdown(ExitReason reason, const QString& detail)
{
    if (m_shutdownRequested.exchange(true)) {
        qInfo() << "[LifecycleSupervisor] Shutdown already requested, ignoring duplicate request";
        return;
    }

    m_exitReason = reason;
    m_exitDetail = detail;

    QString reasonStr = exitReasonToString(reason);
    QString fullReason = detail.isEmpty() ? reasonStr : QString("%1 | %2").arg(reasonStr, detail);

    qWarning() << "[LifecycleSupervisor] Shutdown requested:" << fullReason;
    emit shutdownRequested(reason, detail);

    transitionTo(LifecycleState::ShutdownRequested);

    QMetaObject::invokeMethod(this, &LifecycleSupervisor::executeShutdownPipeline, Qt::QueuedConnection);
}

void LifecycleSupervisor::forceTerminate()
{
    qCritical() << "[LifecycleSupervisor] Force terminating process";
    qCritical() << "[LifecycleSupervisor] Final state:" << stateToString(m_state.load());
    qCritical() << "[LifecycleSupervisor] Exit reason:" << exitReasonToString(m_exitReason);
    qCritical() << "[LifecycleSupervisor] Exit detail:" << m_exitDetail;

    emit forceTerminateRequested();

#ifdef Q_OS_WIN
    TerminateProcess(GetCurrentProcess(), 1);
#else
    std::_Exit(1);
#endif
}

QString LifecycleSupervisor::stateToString(LifecycleState state)
{
    switch (state) {
    case LifecycleState::Running:
        return QStringLiteral("Running");
    case LifecycleState::ShutdownRequested:
        return QStringLiteral("ShutdownRequested");
    case LifecycleState::CancellingTasks:
        return QStringLiteral("CancellingTasks");
    case LifecycleState::ReleasingResources:
        return QStringLiteral("ReleasingResources");
    case LifecycleState::ForceExit:
        return QStringLiteral("ForceExit");
    case LifecycleState::Terminated:
        return QStringLiteral("Terminated");
    }
    return QStringLiteral("Unknown");
}

QString LifecycleSupervisor::exitReasonToString(ExitReason reason)
{
    switch (reason) {
    case ExitReason::Normal:
        return QStringLiteral("normal");
    case ExitReason::MainWindowClosed:
        return QStringLiteral("main_window_closed");
    case ExitReason::MainWindowDestroyed:
        return QStringLiteral("main_window_destroyed");
    case ExitReason::MainWindowHandleLost:
        return QStringLiteral("main_window_handle_lost");
    case ExitReason::MainWindowHidden:
        return QStringLiteral("main_window_hidden");
    case ExitReason::CrashDetected:
        return QStringLiteral("crash_detected");
    case ExitReason::WatchdogTimeout:
        return QStringLiteral("watchdog_timeout");
    case ExitReason::UserRequest:
        return QStringLiteral("user_request");
    case ExitReason::QmlLoadError:
        return QStringLiteral("qml_load_error");
    case ExitReason::BootstrapFailed:
        return QStringLiteral("bootstrap_failed");
    case ExitReason::ExternalFatal:
        return QStringLiteral("external_fatal");
    }
    return QStringLiteral("unknown");
}

QString LifecycleSupervisor::exitReasonString() const
{
    QString result = exitReasonToString(m_exitReason);
    if (!m_exitDetail.isEmpty()) {
        result = QString("%1 | %2").arg(result, m_exitDetail);
    }
    return result;
}

void LifecycleSupervisor::onWindowWatchdogTimeout()
{
    if (isShuttingDown()) {
        return;
    }

    if (!m_mainWindow) {
        return;
    }

    // 检测窗口是否曾经显示过（通过 isVisible 判断）
    if (!m_windowEverShown && m_mainWindow->isVisible()) {
        m_windowEverShown = true;
        qInfo() << "[LifecycleSupervisor] Main window shown detected";
    }

    // 只有窗口曾经显示过后才进行后续检查
    if (!m_windowEverShown) {
        return;
    }

    if (m_mainWindow->isHidden()) {
        qWarning() << "[LifecycleSupervisor] Main window hidden unexpectedly";
        requestShutdown(ExitReason::MainWindowHidden, QStringLiteral("window hidden"));
        return;
    }

    WId winId = m_mainWindow->winId();
    if (winId == 0) {
        qWarning() << "[LifecycleSupervisor] Main window winId is zero";
        requestShutdown(ExitReason::MainWindowHandleLost, QStringLiteral("winId is zero"));
        return;
    }

#ifdef Q_OS_WIN
    HWND hwnd = reinterpret_cast<HWND>(winId);
    if (!IsWindow(hwnd)) {
        qWarning() << "[LifecycleSupervisor] Main window HWND invalid";
        requestShutdown(ExitReason::MainWindowHandleLost, QStringLiteral("HWND invalid"));
        return;
    }
#endif
}

void LifecycleSupervisor::onProcessWatchdogTimeout()
{
    if (isShuttingDown()) {
        return;
    }
}

void LifecycleSupervisor::transitionTo(LifecycleState newState)
{
    LifecycleState oldState = m_state.exchange(newState);
    if (oldState != newState) {
        qInfo() << "[LifecycleSupervisor] State transition:" << stateToString(oldState) << "->" << stateToString(newState);
        emit stateChanged(newState, oldState);
    }
}

void LifecycleSupervisor::executeShutdownPipeline()
{
    qInfo() << "[LifecycleSupervisor] Starting shutdown pipeline";
    qInfo() << "[LifecycleSupervisor] Exit reason:" << exitReasonToString(m_exitReason);

    if (m_windowWatchdog) {
        m_windowWatchdog->stop();
    }
    if (m_processWatchdog) {
        m_processWatchdog->stop();
    }

    transitionTo(LifecycleState::CancellingTasks);

    qInfo() << "[LifecycleSupervisor] Phase 1: Cancelling tasks";
    if (!executePhase(QStringLiteral("CancelTasks"), 2000, [this]() {
        emit cancelAllTasksRequested();
        if (m_processingController) {
            m_processingController->forceCancelAllTasks();
        }
        if (m_threadPool) {
            m_threadPool->clear();
        }
        return true;
    })) {
        qWarning() << "[LifecycleSupervisor] CancelTasks phase timed out";
    }

    transitionTo(LifecycleState::ReleasingResources);

    qInfo() << "[LifecycleSupervisor] Phase 2: Releasing resources";
    if (!executePhase(QStringLiteral("ReleaseResources"), 1000, [this]() {
        emit releaseResourcesRequested();
        return true;
    })) {
        qWarning() << "[LifecycleSupervisor] ReleaseResources phase timed out";
    }

    transitionTo(LifecycleState::ForceExit);

    qInfo() << "[LifecycleSupervisor] Phase 3: Force exit";
    startForceTerminateTimer(500);

    if (auto* app = QCoreApplication::instance()) {
        app->quit();
    }
}

bool LifecycleSupervisor::executePhase(const QString& name, int timeoutMs, std::function<bool()> phaseFunc)
{
    QElapsedTimer timer;
    timer.start();

    qInfo() << "[LifecycleSupervisor] Executing phase:" << name << "timeout:" << timeoutMs << "ms";

    bool result = false;
    try {
        result = phaseFunc();
    } catch (const std::exception& e) {
        qWarning() << "[LifecycleSupervisor] Exception in phase" << name << ":" << e.what();
        return false;
    } catch (...) {
        qWarning() << "[LifecycleSupervisor] Unknown exception in phase" << name;
        return false;
    }

    qint64 elapsed = timer.elapsed();
    qInfo() << "[LifecycleSupervisor] Phase" << name << "completed in" << elapsed << "ms, result:" << result;

    return result;
}

void LifecycleSupervisor::startForceTerminateTimer(int timeoutMs)
{
    if (m_forceTerminateTimer) {
        m_forceTerminateTimer->stop();
        delete m_forceTerminateTimer;
    }

    m_forceTerminateTimer = new QTimer(this);
    m_forceTerminateTimer->setSingleShot(true);
    m_forceTerminateTimer->setInterval(timeoutMs);
    connect(m_forceTerminateTimer, &QTimer::timeout, this, [this]() {
        qCritical() << "[LifecycleSupervisor] Graceful shutdown timed out, forcing termination";
        forceTerminate();
    });
    m_forceTerminateTimer->start();

    qInfo() << "[LifecycleSupervisor] Force terminate timer started (" << timeoutMs << "ms)";
}

} // namespace EnhanceVision
