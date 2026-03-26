/**
 * @file ConcurrencyManager.cpp
 * @brief 并发管理器实现
 * @author Qt客户端开发工程师
 */

#include "EnhanceVision/core/ConcurrencyManager.h"
#include "EnhanceVision/core/TaskTimeoutWatchdog.h"
#include "EnhanceVision/core/TaskRetryPolicy.h"
#include "EnhanceVision/core/PriorityAdjuster.h"
#include "EnhanceVision/core/DeadlockDetector.h"
#include <QDebug>

namespace EnhanceVision {

ConcurrencyManager::ConcurrencyManager(QObject* parent)
    : QObject(parent)
    , m_watchdog(new TaskTimeoutWatchdog(this))
    , m_retryPolicy(new TaskRetryPolicy(this))
    , m_adjuster(new PriorityAdjuster(&m_queue, this))
    , m_deadlockDetector(new DeadlockDetector(this))
{
    connect(m_watchdog, &TaskTimeoutWatchdog::taskTimedOut,
            this, &ConcurrencyManager::taskTimedOut);

    connect(m_retryPolicy, &TaskRetryPolicy::retryScheduled,
            this, &ConcurrencyManager::taskRetryScheduled);
    connect(m_retryPolicy, &TaskRetryPolicy::retriesExhausted,
            this, &ConcurrencyManager::taskRetriesExhausted);

    connect(m_adjuster, &PriorityAdjuster::starvationDetected,
            this, &ConcurrencyManager::starvationDetected);

    connect(m_deadlockDetector, &DeadlockDetector::deadlockDetected,
            this, &ConcurrencyManager::deadlockDetected);
    connect(m_deadlockDetector, &DeadlockDetector::recoveryRequested,
            this, &ConcurrencyManager::deadlockRecoveryRequested);
}

ConcurrencyManager::~ConcurrencyManager()
{
    stop();
}

void ConcurrencyManager::enqueueTask(const PriorityTaskEntry& entry)
{
    m_queue.enqueue(entry);
}

void ConcurrencyManager::removeTask(const QString& taskId)
{
    m_queue.remove(taskId);
    m_watchdog->unwatchTask(taskId);
    m_retryPolicy->resetTask(taskId);
    m_deadlockDetector->removeAllEdgesFor(taskId);
}

bool ConcurrencyManager::hasSchedulableTask(int maxSessions, int maxFilesPerMessage) const
{
    QList<PriorityTaskEntry> items = m_queue.allItems();
    for (const auto& entry : items) {
        if (canScheduleInSession(entry.sessionId, maxSessions, maxFilesPerMessage) &&
            canScheduleInMessage(entry.messageId, maxFilesPerMessage)) {
            return true;
        }
    }
    return false;
}

PriorityTaskEntry ConcurrencyManager::scheduleNext(int maxSessions, int maxFilesPerMessage)
{
    QList<PriorityTaskEntry> items = m_queue.allItems();
    for (const auto& entry : items) {
        if (canScheduleInSession(entry.sessionId, maxSessions, maxFilesPerMessage) &&
            canScheduleInMessage(entry.messageId, maxFilesPerMessage)) {
            m_queue.remove(entry.taskId);
            emit taskScheduled(entry.taskId);
            return entry;
        }
    }
    return {};
}

void ConcurrencyManager::acquireSlot(const QString& taskId, const QString& sessionId, const QString& messageId)
{
    m_activeTasksPerSession[sessionId]++;
    m_activeTasksPerMessage[messageId]++;
    m_taskToSession[taskId] = sessionId;
    m_taskToMessage[taskId] = messageId;
}

void ConcurrencyManager::releaseSlot(const QString& taskId, const QString& sessionId, const QString& messageId)
{
    if (m_activeTasksPerSession.contains(sessionId)) {
        m_activeTasksPerSession[sessionId] = qMax(0, m_activeTasksPerSession[sessionId] - 1);
    }
    if (m_activeTasksPerMessage.contains(messageId)) {
        m_activeTasksPerMessage[messageId] = qMax(0, m_activeTasksPerMessage[messageId] - 1);
    }
    m_taskToSession.remove(taskId);
    m_taskToMessage.remove(taskId);

    m_watchdog->unwatchTask(taskId);
    m_retryPolicy->resetTask(taskId);
    m_deadlockDetector->removeAllEdgesFor(taskId);
}

int ConcurrencyManager::activeTasksForSession(const QString& sessionId) const
{
    return m_activeTasksPerSession.value(sessionId, 0);
}

int ConcurrencyManager::activeTasksForMessage(const QString& messageId) const
{
    return m_activeTasksPerMessage.value(messageId, 0);
}

int ConcurrencyManager::activeSessionCount() const
{
    int count = 0;
    for (auto it = m_activeTasksPerSession.constBegin(); it != m_activeTasksPerSession.constEnd(); ++it) {
        if (it.value() > 0) {
            count++;
        }
    }
    return count;
}

void ConcurrencyManager::onSessionChanged(const QString& activeSessionId)
{
    m_adjuster->onSessionChanged(activeSessionId);
}

PriorityTaskQueue* ConcurrencyManager::queue()
{
    return &m_queue;
}

TaskTimeoutWatchdog* ConcurrencyManager::watchdog()
{
    return m_watchdog;
}

TaskRetryPolicy* ConcurrencyManager::retryPolicy()
{
    return m_retryPolicy;
}

PriorityAdjuster* ConcurrencyManager::adjuster()
{
    return m_adjuster;
}

DeadlockDetector* ConcurrencyManager::deadlockDetector()
{
    return m_deadlockDetector;
}

void ConcurrencyManager::start()
{
    m_adjuster->start();
    m_deadlockDetector->start();
}

void ConcurrencyManager::stop()
{
    m_adjuster->stop();
    m_deadlockDetector->stop();
    m_watchdog->stop();
}

int ConcurrencyManager::pendingTaskCount() const
{
    return m_queue.size();
}

int ConcurrencyManager::totalActiveSlots() const
{
    int total = 0;
    for (auto it = m_activeTasksPerSession.constBegin(); it != m_activeTasksPerSession.constEnd(); ++it) {
        total += it.value();
    }
    return total;
}

bool ConcurrencyManager::canScheduleInSession(const QString& sessionId, int maxSessions, int /*maxFilesPerMessage*/) const
{
    int sessionActive = m_activeTasksPerSession.value(sessionId, 0);
    if (sessionActive > 0) {
        return true;
    }
    return activeSessionCount() < maxSessions;
}

bool ConcurrencyManager::canScheduleInMessage(const QString& messageId, int maxFilesPerMessage) const
{
    return m_activeTasksPerMessage.value(messageId, 0) < maxFilesPerMessage;
}

} // namespace EnhanceVision
