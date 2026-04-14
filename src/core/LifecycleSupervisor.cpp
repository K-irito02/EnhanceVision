#include "EnhanceVision/core/LifecycleSupervisor.h"
#include "EnhanceVision/controllers/ProcessingController.h"
#include "EnhanceVision/controllers/SettingsController.h"
#include <QWindow>
#include <QThreadPool>
#include <QCoreApplication>
#include <QElapsedTimer>
#include <QEventLoop>
#include <QGuiApplication>
#include <QDebug>
#include <chrono>
#include <thread>

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

LifecycleSupervisor* LifecycleSupervisor::instanceIfExists()
{
    return s_instance;
}

LifecycleSupervisor::LifecycleSupervisor(QObject* parent)
    : QObject(parent)
{
}

LifecycleSupervisor::~LifecycleSupervisor()
{
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

void LifecycleSupervisor::setupWindowWatchdog(QWindow* mainWindow)
{
    if (!mainWindow) {
        qWarning() << "[LifecycleSupervisor] Cannot setup window watchdog: mainWindow is null";
        return;
    }

    m_mainWindow = mainWindow;
    m_windowEverShown = false;

    connect(mainWindow, &QObject::destroyed, this, [this]() {
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
}

void LifecycleSupervisor::setupProcessWatchdog()
{
    m_processWatchdog = new QTimer(this);
    m_processWatchdog->setInterval(1000);
    connect(m_processWatchdog, &QTimer::timeout, this, &LifecycleSupervisor::onProcessWatchdogTimeout);
    m_processWatchdog->start();
}

void LifecycleSupervisor::requestShutdown(ExitReason reason, const QString& detail)
{
    if (m_shutdownRequested.exchange(true)) {
        return;
    }

    m_exitReason = reason;
    m_exitDetail = detail;

    QString reasonStr = exitReasonToString(reason);
    QString fullReason = detail.isEmpty() ? reasonStr : QString("%1 | %2").arg(reasonStr, detail);

    qInfo() << "[LifecycleSupervisor] Shutdown requested:" << fullReason;

    bool isNormalExit = (reason == ExitReason::Normal ||
                         reason == ExitReason::MainWindowClosed ||
                         reason == ExitReason::UserRequest);

    if (isNormalExit) {
        SettingsController::instance()->markNormalExit(reasonStr);
    }

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

void LifecycleSupervisor::disarmForceTerminate()
{
    const bool wasArmed = m_forceTerminateArmed.exchange(false);
    m_forceTerminateGeneration.fetch_add(1);
}

void LifecycleSupervisor::markTerminationComplete()
{
    transitionTo(LifecycleState::Terminated);
    disarmForceTerminate();
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
    }

    // 只有窗口曾经显示过后才进行后续检查
    if (!m_windowEverShown) {
        return;
    }

    if (m_mainWindow->visibility() == QWindow::Hidden) {
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
        emit stateChanged(newState, oldState);
    }
}

void LifecycleSupervisor::closeTopLevelWindows()
{
    const auto windows = QGuiApplication::topLevelWindows();
    for (QWindow* window : windows) {
        if (!window || window == m_mainWindow) {
            continue;
        }

        window->close();
    }
}

void LifecycleSupervisor::executeShutdownPipeline()
{
    if (m_windowWatchdog) {
        m_windowWatchdog->stop();
    }
    if (m_processWatchdog) {
        m_processWatchdog->stop();
    }

    closeTopLevelWindows();

    transitionTo(LifecycleState::CancellingTasks);

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

    if (!executePhase(QStringLiteral("ReleaseResources"), 1000, [this]() {
        emit releaseResourcesRequested();
        return true;
    })) {
        qWarning() << "[LifecycleSupervisor] ReleaseResources phase timed out";
    }

    transitionTo(LifecycleState::ForceExit);

    armForceTerminate(2000);

    if (auto* app = QCoreApplication::instance()) {
        app->quit();
    }
}

bool LifecycleSupervisor::executePhase(const QString& name, int timeoutMs, std::function<bool()> phaseFunc)
{
    QElapsedTimer timer;
    timer.start();

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

    return result;
}

void LifecycleSupervisor::armForceTerminate(int timeoutMs)
{
    if (timeoutMs <= 0) {
        return;
    }

    const std::uint64_t generation = m_forceTerminateGeneration.fetch_add(1) + 1;
    m_forceTerminateArmed.store(true);

    std::thread([this, generation, timeoutMs]() {
        std::this_thread::sleep_for(std::chrono::milliseconds(timeoutMs));
        if (!m_forceTerminateArmed.load()) {
            return;
        }
        if (m_forceTerminateGeneration.load() != generation) {
            return;
        }

        qCritical() << "[LifecycleSupervisor] Graceful shutdown timed out, forcing termination";
        forceTerminate();
    }).detach();
}

} // namespace EnhanceVision
