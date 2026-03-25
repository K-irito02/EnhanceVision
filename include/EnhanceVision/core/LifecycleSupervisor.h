#ifndef ENHANCEVISION_LIFECYCLE_SUPERVISOR_H
#define ENHANCEVISION_LIFECYCLE_SUPERVISOR_H

#include <QObject>
#include <QTimer>
#include <QString>
#include <functional>
#include <atomic>

class QWidget;
class QThreadPool;

namespace EnhanceVision {

class ProcessingController;

enum class LifecycleState {
    Running,
    ShutdownRequested,
    CancellingTasks,
    ReleasingResources,
    ForceExit,
    Terminated
};

enum class ExitReason {
    Normal,
    MainWindowClosed,
    MainWindowDestroyed,
    MainWindowHandleLost,
    MainWindowHidden,
    CrashDetected,
    WatchdogTimeout,
    UserRequest,
    QmlLoadError,
    BootstrapFailed,
    ExternalFatal
};

struct ShutdownPhase {
    QString name;
    int timeoutMs;
    std::function<bool()> execute;
    bool completed = false;
};

class LifecycleSupervisor : public QObject
{
    Q_OBJECT

public:
    static LifecycleSupervisor* instance();

    void setProcessingController(ProcessingController* controller);
    void setThreadPool(QThreadPool* pool);

    void setupWindowWatchdog(QWidget* mainWindow);
    void setupProcessWatchdog();

    void requestShutdown(ExitReason reason, const QString& detail = QString());
    void forceTerminate();

    LifecycleState state() const { return m_state.load(); }
    ExitReason exitReason() const { return m_exitReason; }
    QString exitReasonString() const;
    bool isShuttingDown() const { return m_state.load() != LifecycleState::Running; }

    static QString stateToString(LifecycleState state);
    static QString exitReasonToString(ExitReason reason);

signals:
    void shutdownRequested(ExitReason reason, const QString& detail);
    void stateChanged(LifecycleState newState, LifecycleState oldState);
    void cancelAllTasksRequested();
    void releaseResourcesRequested();
    void forceTerminateRequested();

private slots:
    void onWindowWatchdogTimeout();
    void onProcessWatchdogTimeout();

private:
    explicit LifecycleSupervisor(QObject* parent = nullptr);
    ~LifecycleSupervisor() override;

    void transitionTo(LifecycleState newState);
    void executeShutdownPipeline();
    bool executePhase(const QString& name, int timeoutMs, std::function<bool()> phaseFunc);
    void startForceTerminateTimer(int timeoutMs);

    static LifecycleSupervisor* s_instance;

    std::atomic<LifecycleState> m_state{LifecycleState::Running};
    ExitReason m_exitReason{ExitReason::Normal};
    QString m_exitDetail;

    QWidget* m_mainWindow = nullptr;
    ProcessingController* m_processingController = nullptr;
    QThreadPool* m_threadPool = nullptr;

    QTimer* m_windowWatchdog = nullptr;
    QTimer* m_processWatchdog = nullptr;
    QTimer* m_forceTerminateTimer = nullptr;

    bool m_windowEverShown = false;
    std::atomic<bool> m_shutdownRequested{false};
};

} // namespace EnhanceVision

#endif // ENHANCEVISION_LIFECYCLE_SUPERVISOR_H
