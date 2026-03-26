/**
 * @file ConcurrencyMonitor.cpp
 * @brief 并发监控器实现
 * @author Qt客户端开发工程师
 */

#include "EnhanceVision/core/ConcurrencyMonitor.h"
#include "EnhanceVision/core/ConcurrencyManager.h"
#include "EnhanceVision/core/ResourceManager.h"
#include "EnhanceVision/core/TaskTimeoutWatchdog.h"
#include "EnhanceVision/core/DeadlockDetector.h"
#include <QDebug>
#include <algorithm>

namespace EnhanceVision {

ConcurrencyMonitor::ConcurrencyMonitor(QObject* parent)
    : QObject(parent)
    , m_snapshotTimer(new QTimer(this))
{
    m_snapshotTimer->setTimerType(Qt::CoarseTimer);
    connect(m_snapshotTimer, &QTimer::timeout, this, &ConcurrencyMonitor::onSnapshotTimer);
    m_monitorStartTime.start();
}

ConcurrencyMonitor::~ConcurrencyMonitor()
{
    stop();
}

void ConcurrencyMonitor::setConcurrencyManager(ConcurrencyManager* mgr)
{
    m_concurrencyManager = mgr;
}

void ConcurrencyMonitor::setResourceManager(ResourceManager* resMgr)
{
    m_resourceManager = resMgr;
}

void ConcurrencyMonitor::setAiPoolInfo(int active, int available)
{
    m_aiPoolActive = active;
    m_aiPoolAvailable = available;
}

void ConcurrencyMonitor::start(int intervalMs)
{
    if (!m_snapshotTimer->isActive()) {
        m_snapshotTimer->start(qMax(1000, intervalMs));
    }
}

void ConcurrencyMonitor::stop()
{
    m_snapshotTimer->stop();
}

bool ConcurrencyMonitor::isRunning() const
{
    return m_snapshotTimer->isActive();
}

ConcurrencyMonitor::Snapshot ConcurrencyMonitor::takeSnapshot() const
{
    Snapshot snap;
    snap.timestamp = QDateTime::currentDateTime();

    if (m_concurrencyManager) {
        snap.pendingTasks = m_concurrencyManager->pendingTaskCount();
        snap.activeTasks = m_concurrencyManager->totalActiveSlots();
        snap.activeSessions = m_concurrencyManager->activeSessionCount();
        snap.watchdogTrackedTasks = m_concurrencyManager->watchdog()->watchedTaskCount();
        snap.deadlockEdges = m_concurrencyManager->deadlockDetector()->edgeCount();
    }

    if (m_resourceManager) {
        snap.systemMemoryUsedMB = m_resourceManager->usedMemoryMB();
        snap.systemMemoryAvailMB = m_resourceManager->availableSystemMemoryMB();
        snap.gpuMemoryAvailMB = m_resourceManager->availableGpuMemoryMB();
        snap.resourcePressure = static_cast<int>(m_resourceManager->pressureLevel());
    }

    snap.aiEnginePoolActive = m_aiPoolActive;
    snap.aiEnginePoolAvailable = m_aiPoolAvailable;

    return snap;
}

ConcurrencyMonitor::AggregateMetrics ConcurrencyMonitor::aggregateMetrics() const
{
    AggregateMetrics agg;

    if (m_completedMetrics.isEmpty()) {
        return agg;
    }

    qint64 totalWait = 0;
    qint64 totalProc = 0;
    QList<qint64> procTimes;

    for (const auto& m : m_completedMetrics) {
        if (m.succeeded) {
            agg.totalTasksCompleted++;
        } else {
            agg.totalTasksFailed++;
        }
        totalWait += m.waitTimeMs;
        totalProc += m.processingTimeMs;
        procTimes.append(m.processingTimeMs);

        if (m.processingTimeMs > agg.maxProcessingTimeMs) {
            agg.maxProcessingTimeMs = m.processingTimeMs;
        }
    }

    int total = m_completedMetrics.size();
    agg.avgWaitTimeMs = totalWait / total;
    agg.avgProcessingTimeMs = totalProc / total;

    std::sort(procTimes.begin(), procTimes.end());
    int p95Index = static_cast<int>(procTimes.size() * 0.95);
    if (p95Index >= procTimes.size()) p95Index = procTimes.size() - 1;
    agg.p95ProcessingTimeMs = procTimes[p95Index];

    qint64 elapsedMinutes = m_monitorStartTime.elapsed() / 60000;
    if (elapsedMinutes > 0) {
        agg.throughputPerMinute = static_cast<double>(agg.totalTasksCompleted) / elapsedMinutes;
    }

    return agg;
}

void ConcurrencyMonitor::recordTaskStarted(const QString& taskId)
{
    QElapsedTimer timer;
    timer.start();
    m_taskStartTimers[taskId] = timer;
}

void ConcurrencyMonitor::recordTaskCompleted(const QString& taskId)
{
    TaskMetrics metrics;
    metrics.taskId = taskId;
    metrics.succeeded = true;

    if (m_taskStartTimers.contains(taskId)) {
        metrics.processingTimeMs = m_taskStartTimers[taskId].elapsed();
        m_taskStartTimers.remove(taskId);
    }
    metrics.waitTimeMs = m_taskWaitTimes.value(taskId, 0);
    m_taskWaitTimes.remove(taskId);

    m_completedMetrics.append(metrics);
    if (m_completedMetrics.size() > kMaxMetrics) {
        m_completedMetrics.removeFirst();
    }
}

void ConcurrencyMonitor::recordTaskFailed(const QString& taskId)
{
    TaskMetrics metrics;
    metrics.taskId = taskId;
    metrics.succeeded = false;

    if (m_taskStartTimers.contains(taskId)) {
        metrics.processingTimeMs = m_taskStartTimers[taskId].elapsed();
        m_taskStartTimers.remove(taskId);
    }
    metrics.waitTimeMs = m_taskWaitTimes.value(taskId, 0);
    m_taskWaitTimes.remove(taskId);

    m_completedMetrics.append(metrics);
    if (m_completedMetrics.size() > kMaxMetrics) {
        m_completedMetrics.removeFirst();
    }
}

QList<ConcurrencyMonitor::Snapshot> ConcurrencyMonitor::recentSnapshots(int count) const
{
    if (count >= m_snapshots.size()) {
        return m_snapshots;
    }
    return m_snapshots.mid(m_snapshots.size() - count);
}

void ConcurrencyMonitor::onSnapshotTimer()
{
    Snapshot snap = takeSnapshot();

    m_snapshots.append(snap);
    if (m_snapshots.size() > kMaxSnapshots) {
        m_snapshots.removeFirst();
    }

    emit snapshotTaken(snap);
    detectAnomalies(snap);
}

void ConcurrencyMonitor::detectAnomalies(const Snapshot& snapshot)
{
    if (snapshot.resourcePressure >= 2) {
        emit anomalyDetected(QStringLiteral("High resource pressure detected (level %1)")
                             .arg(snapshot.resourcePressure));
    }

    if (snapshot.deadlockEdges > 0) {
        emit anomalyDetected(QStringLiteral("Deadlock wait edges present: %1")
                             .arg(snapshot.deadlockEdges));
    }

    if (snapshot.pendingTasks > 50) {
        emit anomalyDetected(QStringLiteral("Large pending queue: %1 tasks")
                             .arg(snapshot.pendingTasks));
    }

    if (snapshot.activeTasks == 0 && snapshot.pendingTasks > 0) {
        emit anomalyDetected(QStringLiteral("Stall detected: %1 pending but 0 active")
                             .arg(snapshot.pendingTasks));
    }
}

} // namespace EnhanceVision
