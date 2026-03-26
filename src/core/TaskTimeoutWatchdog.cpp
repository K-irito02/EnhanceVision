/**
 * @file TaskTimeoutWatchdog.cpp
 * @brief 任务超时看门狗实现
 * @author Qt客户端开发工程师
 */

#include "EnhanceVision/core/TaskTimeoutWatchdog.h"
#include <QDateTime>
#include <QDebug>

namespace EnhanceVision {

TaskTimeoutWatchdog::TaskTimeoutWatchdog(QObject* parent)
    : QObject(parent)
    , m_checkTimer(new QTimer(this))
{
    m_checkTimer->setTimerType(Qt::CoarseTimer);
    connect(m_checkTimer, &QTimer::timeout, this, &TaskTimeoutWatchdog::performCheck);
}

TaskTimeoutWatchdog::~TaskTimeoutWatchdog()
{
    stop();
}

void TaskTimeoutWatchdog::watchTask(const QString& taskId, int timeoutMs)
{
    const qint64 nowMs = QDateTime::currentMSecsSinceEpoch();
    WatchEntry entry;
    entry.startMs = nowMs;
    entry.lastProgressMs = nowMs;
    entry.timeoutMs = (timeoutMs > 0) ? timeoutMs : m_defaultTimeoutMs;
    entry.stallWarned = false;
    m_watches[taskId] = entry;

    if (!m_checkTimer->isActive() && !m_watches.isEmpty()) {
        m_checkTimer->start(m_checkIntervalMs);
    }
}

void TaskTimeoutWatchdog::unwatchTask(const QString& taskId)
{
    m_watches.remove(taskId);

    if (m_watches.isEmpty() && m_checkTimer->isActive()) {
        m_checkTimer->stop();
    }
}

void TaskTimeoutWatchdog::resetTimer(const QString& taskId)
{
    if (m_watches.contains(taskId)) {
        m_watches[taskId].lastProgressMs = QDateTime::currentMSecsSinceEpoch();
        m_watches[taskId].stallWarned = false;
    }
}

void TaskTimeoutWatchdog::setDefaultTimeout(int timeoutMs)
{
    m_defaultTimeoutMs = qMax(1000, timeoutMs);
}

int TaskTimeoutWatchdog::defaultTimeout() const
{
    return m_defaultTimeoutMs;
}

void TaskTimeoutWatchdog::setStallThreshold(int stallMs)
{
    m_stallThresholdMs = qMax(1000, stallMs);
}

int TaskTimeoutWatchdog::stallThreshold() const
{
    return m_stallThresholdMs;
}

void TaskTimeoutWatchdog::setCheckInterval(int intervalMs)
{
    m_checkIntervalMs = qMax(500, intervalMs);
    if (m_checkTimer->isActive()) {
        m_checkTimer->setInterval(m_checkIntervalMs);
    }
}

int TaskTimeoutWatchdog::checkInterval() const
{
    return m_checkIntervalMs;
}

void TaskTimeoutWatchdog::start()
{
    if (!m_checkTimer->isActive() && !m_watches.isEmpty()) {
        m_checkTimer->start(m_checkIntervalMs);
    }
}

void TaskTimeoutWatchdog::stop()
{
    m_checkTimer->stop();
}

bool TaskTimeoutWatchdog::isRunning() const
{
    return m_checkTimer->isActive();
}

int TaskTimeoutWatchdog::watchedTaskCount() const
{
    return m_watches.size();
}

void TaskTimeoutWatchdog::performCheck()
{
    if (m_watches.isEmpty()) {
        m_checkTimer->stop();
        return;
    }

    const qint64 nowMs = QDateTime::currentMSecsSinceEpoch();
    QStringList timedOutTasks;

    for (auto it = m_watches.begin(); it != m_watches.end(); ++it) {
        const QString& taskId = it.key();
        WatchEntry& entry = it.value();

        const qint64 elapsed = nowMs - entry.startMs;
        const qint64 sinceLast = nowMs - entry.lastProgressMs;

        if (elapsed >= entry.timeoutMs) {
            qWarning() << "[Watchdog] task timed out"
                        << "task:" << taskId
                        << "elapsed:" << elapsed << "ms"
                        << "timeout:" << entry.timeoutMs << "ms";
            timedOutTasks.append(taskId);
            continue;
        }

        if (sinceLast >= m_stallThresholdMs && !entry.stallWarned) {
            entry.stallWarned = true;
            qWarning() << "[Watchdog] task stalled"
                        << "task:" << taskId
                        << "no progress for:" << sinceLast << "ms"
                        << "threshold:" << m_stallThresholdMs << "ms";
            emit taskStalled(taskId, sinceLast);
        }
    }

    for (const QString& taskId : timedOutTasks) {
        m_watches.remove(taskId);
        emit taskTimedOut(taskId);
    }

    if (m_watches.isEmpty()) {
        m_checkTimer->stop();
    }
}

} // namespace EnhanceVision
