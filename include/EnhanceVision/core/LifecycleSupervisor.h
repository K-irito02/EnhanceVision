#ifndef ENHANCEVISION_LIFECYCLE_SUPERVISOR_H
#define ENHANCEVISION_LIFECYCLE_SUPERVISOR_H

#include <QObject>
#include <QTimer>
#include <QString>
#include <functional>
#include <atomic>
#include <cstdint>

class QWindow;
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
    static LifecycleSupervisor* instanceIfExists();

    void setProcessingController(ProcessingController* controller);
    void setThreadPool(QThreadPool* pool);

    void setupWindowWatchdog(QWindow* mainWindow);
    void setupProcessWatchdog();

    void requestShutdown(ExitReason reason, const QString& detail = QString());
    void forceTerminate();
    void disarmForceTerminate();
    void markTerminationComplete();

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
    void closeTopLevelWindows();
    bool executePhase(const QString& name, int timeoutMs, std::function<bool()> phaseFunc);
    void armForceTerminate(int timeoutMs);

    static LifecycleSupervisor* s_instance;

    std::atomic<LifecycleState> m_state{LifecycleState::Running};
    ExitReason m_exitReason{ExitReason::Normal};
    QString m_exitDetail;

    QWindow* m_mainWindow = nullptr;
    ProcessingController* m_processingController = nullptr;
    QThreadPool* m_threadPool = nullptr;

    QTimer* m_windowWatchdog = nullptr;
    QTimer* m_processWatchdog = nullptr;

    bool m_windowEverShown = false;
    std::atomic<bool> m_shutdownRequested{false};
    std::atomic<bool> m_forceTerminateArmed{false};
    std::atomic<std::uint64_t> m_forceTerminateGeneration{0};
};

} // namespace EnhanceVision

#endif // ENHANCEVISION_LIFECYCLE_SUPERVISOR_H
