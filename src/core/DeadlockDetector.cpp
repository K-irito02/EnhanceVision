/**
 * @file DeadlockDetector.cpp
 * @brief 死锁检测器实现
 * @author Qt客户端开发工程师
 */

#include "EnhanceVision/core/DeadlockDetector.h"
#include <QDebug>

namespace EnhanceVision {

DeadlockDetector::DeadlockDetector(QObject* parent)
    : QObject(parent)
    , m_checkTimer(new QTimer(this))
{
    m_checkTimer->setTimerType(Qt::CoarseTimer);
    connect(m_checkTimer, &QTimer::timeout, this, &DeadlockDetector::performCheck);
}

DeadlockDetector::~DeadlockDetector()
{
    stop();
}

void DeadlockDetector::addWaitEdge(const QString& waiterId, const QString& holderId, const QString& resourceId)
{
    WaitEdge edge;
    edge.waiterId = waiterId;
    edge.holderId = holderId;
    edge.resourceId = resourceId;
    edge.createdAt = QDateTime::currentDateTime();
    m_waitGraph[waiterId].append(edge);
}

void DeadlockDetector::removeWaitEdge(const QString& waiterId, const QString& resourceId)
{
    if (!m_waitGraph.contains(waiterId)) {
        return;
    }
    auto& edges = m_waitGraph[waiterId];
    for (int i = edges.size() - 1; i >= 0; --i) {
        if (edges[i].resourceId == resourceId) {
            edges.removeAt(i);
        }
    }
    if (edges.isEmpty()) {
        m_waitGraph.remove(waiterId);
    }
}

void DeadlockDetector::removeAllEdgesFor(const QString& taskId)
{
    m_waitGraph.remove(taskId);

    for (auto it = m_waitGraph.begin(); it != m_waitGraph.end(); ) {
        auto& edges = it.value();
        for (int i = edges.size() - 1; i >= 0; --i) {
            if (edges[i].holderId == taskId) {
                edges.removeAt(i);
            }
        }
        if (edges.isEmpty()) {
            it = m_waitGraph.erase(it);
        } else {
            ++it;
        }
    }
}

bool DeadlockDetector::detectCycle() const
{
    QSet<QString> visited;
    QSet<QString> inStack;
    QStringList cyclePath;

    for (auto it = m_waitGraph.constBegin(); it != m_waitGraph.constEnd(); ++it) {
        if (!visited.contains(it.key())) {
            if (dfs(it.key(), visited, inStack, cyclePath)) {
                return true;
            }
        }
    }
    return false;
}

QStringList DeadlockDetector::getCycleParticipants() const
{
    QSet<QString> visited;
    QSet<QString> inStack;
    QStringList cyclePath;

    for (auto it = m_waitGraph.constBegin(); it != m_waitGraph.constEnd(); ++it) {
        if (!visited.contains(it.key())) {
            if (dfs(it.key(), visited, inStack, cyclePath)) {
                return cyclePath;
            }
        }
    }
    return {};
}

void DeadlockDetector::setCheckInterval(int intervalMs)
{
    m_checkIntervalMs = qMax(1000, intervalMs);
    if (m_checkTimer->isActive()) {
        m_checkTimer->setInterval(m_checkIntervalMs);
    }
}

int DeadlockDetector::checkInterval() const
{
    return m_checkIntervalMs;
}

void DeadlockDetector::setRecoveryStrategy(RecoveryStrategy strategy)
{
    m_strategy = strategy;
}

DeadlockDetector::RecoveryStrategy DeadlockDetector::recoveryStrategy() const
{
    return m_strategy;
}

void DeadlockDetector::start()
{
    if (!m_checkTimer->isActive()) {
        m_checkTimer->start(m_checkIntervalMs);
    }
}

void DeadlockDetector::stop()
{
    m_checkTimer->stop();
}

bool DeadlockDetector::isRunning() const
{
    return m_checkTimer->isActive();
}

int DeadlockDetector::edgeCount() const
{
    int count = 0;
    for (auto it = m_waitGraph.constBegin(); it != m_waitGraph.constEnd(); ++it) {
        count += it.value().size();
    }
    return count;
}

void DeadlockDetector::performCheck()
{
    if (m_waitGraph.isEmpty()) {
        return;
    }

    QStringList participants = getCycleParticipants();
    if (participants.isEmpty()) {
        return;
    }

    qWarning() << "[DeadlockDetector] deadlock detected, participants:" << participants;
    emit deadlockDetected(participants);

    QString victim = selectVictim(participants);
    if (!victim.isEmpty()) {
        qWarning() << "[DeadlockDetector] requesting recovery, cancel task:" << victim;
        emit recoveryRequested(victim);
    }
}

bool DeadlockDetector::dfs(const QString& node, QSet<QString>& visited, QSet<QString>& inStack,
                            QStringList& cyclePath) const
{
    visited.insert(node);
    inStack.insert(node);

    if (m_waitGraph.contains(node)) {
        for (const auto& edge : m_waitGraph[node]) {
            const QString& neighbor = edge.holderId;
            if (!visited.contains(neighbor)) {
                if (dfs(neighbor, visited, inStack, cyclePath)) {
                    cyclePath.prepend(node);
                    return true;
                }
            } else if (inStack.contains(neighbor)) {
                cyclePath.append(neighbor);
                cyclePath.prepend(node);
                return true;
            }
        }
    }

    inStack.remove(node);
    return false;
}

QString DeadlockDetector::selectVictim(const QStringList& participants) const
{
    if (participants.isEmpty()) {
        return {};
    }

    switch (m_strategy) {
    case RecoveryStrategy::CancelYoungest: {
        QString youngest;
        QDateTime youngestTime;
        for (const auto& taskId : participants) {
            if (m_waitGraph.contains(taskId)) {
                for (const auto& edge : m_waitGraph[taskId]) {
                    if (youngest.isEmpty() || edge.createdAt > youngestTime) {
                        youngest = taskId;
                        youngestTime = edge.createdAt;
                    }
                }
            }
        }
        return youngest.isEmpty() ? participants.last() : youngest;
    }
    case RecoveryStrategy::CancelLowestPriority:
        return participants.last();
    case RecoveryStrategy::CancelAll:
        return participants.first();
    }
    return participants.last();
}

} // namespace EnhanceVision
