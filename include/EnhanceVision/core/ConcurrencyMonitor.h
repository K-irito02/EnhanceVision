/**
 * @file ConcurrencyMonitor.h
 * @brief 并发监控器 - 实时统计、性能指标收集、异常检测
 * @author Qt客户端开发工程师
 */

#ifndef ENHANCEVISION_CONCURRENCYMONITOR_H
#define ENHANCEVISION_CONCURRENCYMONITOR_H

#include <QObject>
#include <QTimer>
#include <QHash>
#include <QString>
#include <QDateTime>
#include <QElapsedTimer>

namespace EnhanceVision {

class ConcurrencyManager;
class ResourceManager;

class ConcurrencyMonitor : public QObject
{
    Q_OBJECT

public:
    struct Snapshot {
        QDateTime timestamp;
        int pendingTasks = 0;
        int activeTasks = 0;
        int activeSessions = 0;
        int aiEnginePoolActive = 0;
        int aiEnginePoolAvailable = 0;
        qint64 systemMemoryUsedMB = 0;
        qint64 systemMemoryAvailMB = 0;
        qint64 gpuMemoryAvailMB = 0;
        int resourcePressure = 0;
        int watchdogTrackedTasks = 0;
        int deadlockEdges = 0;
    };

    struct TaskMetrics {
        QString taskId;
        qint64 waitTimeMs = 0;
        qint64 processingTimeMs = 0;
        bool succeeded = false;
    };

    struct AggregateMetrics {
        int totalTasksCompleted = 0;
        int totalTasksFailed = 0;
        qint64 avgWaitTimeMs = 0;
        qint64 avgProcessingTimeMs = 0;
        qint64 p95ProcessingTimeMs = 0;
        qint64 maxProcessingTimeMs = 0;
        double throughputPerMinute = 0.0;
    };

    explicit ConcurrencyMonitor(QObject* parent = nullptr);
    ~ConcurrencyMonitor() override;

    void setConcurrencyManager(ConcurrencyManager* mgr);
    void setResourceManager(ResourceManager* resMgr);
    void setAiPoolInfo(int active, int available);

    void start(int intervalMs = 5000);
    void stop();
    bool isRunning() const;

    Snapshot takeSnapshot() const;
    AggregateMetrics aggregateMetrics() const;

    void recordTaskStarted(const QString& taskId);
    void recordTaskCompleted(const QString& taskId);
    void recordTaskFailed(const QString& taskId);

    QList<Snapshot> recentSnapshots(int count = 10) const;

signals:
    void snapshotTaken(const Snapshot& snapshot);
    void anomalyDetected(const QString& description);

private slots:
    void onSnapshotTimer();

private:
    void detectAnomalies(const Snapshot& snapshot);

    ConcurrencyManager* m_concurrencyManager = nullptr;
    ResourceManager* m_resourceManager = nullptr;
    QTimer* m_snapshotTimer;

    int m_aiPoolActive = 0;
    int m_aiPoolAvailable = 0;

    QHash<QString, QElapsedTimer> m_taskStartTimers;
    QHash<QString, qint64> m_taskWaitTimes;
    QList<TaskMetrics> m_completedMetrics;
    QList<Snapshot> m_snapshots;

    static constexpr int kMaxSnapshots = 120;
    static constexpr int kMaxMetrics = 1000;

    QElapsedTimer m_monitorStartTime;
};

} // namespace EnhanceVision

#endif // ENHANCEVISION_CONCURRENCYMONITOR_H
